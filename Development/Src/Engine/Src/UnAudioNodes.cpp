/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h" 
#include "EngineAudioDeviceClasses.h"
#include "EngineSoundClasses.h"
#include "UnSubtitleManager.h"

#if XBOX
#include "XAudio2Device.h"
#endif

// Some predefined FNames
static FName SpokenTextFName = FName( TEXT( "SpokenText" ) );
static FName TTSSpeakerFName = FName( TEXT( "TTSSpeaker" ) );
static FName bUseTTSFName = FName( TEXT( "bUseTTS" ) );
static FName CompressionQualityFName = FName( TEXT( "CompressionQuality" ) );
static FName LoopingSoundFName = FName( TEXT( "bLoopingSound" ) );
static FName ForceRealTimeDecompressionFName = FName( TEXT( "bForceRealTimeDecompression" ) );

//@warning: DO NOT access variables declared via DECLARE_SOUNDNODE_ELEMENT* after calls to ParseNodes as they
//@warning: might cause the array to be realloc'ed and hence will point to garbage data until the declaration.

/*-----------------------------------------------------------------------------
	USoundCue implementation.
-----------------------------------------------------------------------------*/

/**
 * Recursively finds sound nodes of type T
 */
template<typename T> void USoundCue::RecursiveFindNode( USoundNode* Node, TArray<T*>& OutNodes )
{
	if( Node )
	{
		// Record the node if it is attenuation
		if( Node->IsA( T::StaticClass() ) )
		{
			OutNodes.AddUniqueItem( static_cast<T*>( Node ) );
		}

		// Recurse.
		const INT MaxChildNodes = Node->GetMaxChildNodes();
		for( INT ChildIndex = 0 ; ChildIndex < Node->ChildNodes.Num() && ( ChildIndex < MaxChildNodes || MaxChildNodes == -1 ); ++ChildIndex )
		{
			RecursiveFindNode<T>( Node->ChildNodes( ChildIndex ), OutNodes );
		}
	}
}

template void USoundCue::RecursiveFindNode(USoundNode* Node, TArray<USoundNodeWave*>& OutNodes); 
template void USoundCue::RecursiveFindNode(USoundNode* Node, TArray<USoundNode*>& OutNodes); 

void USoundCue::RecursiveFindMixer( USoundNode* Node, TArray<class USoundNodeMixer*> &OutNodes )
{
	RecursiveFindNode<USoundNodeMixer>( Node, OutNodes );
}

void USoundCue::RecursiveFindAttenuation( USoundNode* Node, TArray<class USoundNodeAttenuation*> &OutNodes )
{
	RecursiveFindNode<USoundNodeAttenuation>( Node, OutNodes );
}

/**
 * Recursively finds all Nodes in the Tree
 */
void USoundCue::RecursiveFindAllNodes( USoundNode* Node, TArray<class USoundNode*> &OutNodes )
{
	if( Node )
	{
		// Record the node if it is a wave.
		if( Node->IsA( USoundNode::StaticClass() ) )
		{
			OutNodes.AddUniqueItem( static_cast<USoundNode*>( Node ) );
		}

		// Recurse.
		const INT MaxChildNodes = Node->GetMaxChildNodes();
		for( INT ChildIndex = 0 ; ChildIndex < Node->ChildNodes.Num() && ( ChildIndex < MaxChildNodes || MaxChildNodes == -1 ); ++ChildIndex )
		{
			RecursiveFindAllNodes( Node->ChildNodes( ChildIndex ), OutNodes );
		}
	}
}

/**
 * Returns a description of this object that can be used as extra information in list views.
 */
FString USoundCue::GetDesc( void )
{
	FString Description = TEXT( "" );

	// Display duration
	if( GetCueDuration() < INDEFINITELY_LOOPING_DURATION )
	{
		Description = FString::Printf( TEXT( "%3.2fs" ), GetCueDuration() );
	}
	else
	{
		Description = TEXT( "Forever" );
	}

	// Display group
	Description += TEXT( " [" );
	Description += *SoundClass.ToString();
	Description += TEXT( "]" );

	return Description;
}

/** 
 * Returns detailed info to populate listview columns
 */
FString USoundCue::GetDetailedDescription( INT InIndex )
{
	FString Description = TEXT( "" );
	switch( InIndex )
	{
	case 0:
		Description = *SoundClass.ToString();
		break;
	case 3:
		if( GetCueDuration() < INDEFINITELY_LOOPING_DURATION )
		{
			Description = FString::Printf( TEXT( "%2.2f Sec" ), GetCueDuration() );
		}
		else
		{
			Description = TEXT( "Forever" );
		}
		break;
	case 8:
		TArray<USoundNodeWave*> Waves;

		RecursiveFindNode<USoundNodeWave>( FirstNode, Waves );

		Description = TEXT( "<no subtitles>" );
		if( Waves.Num() > 0 )
		{
			USoundNodeWave* Wave = Waves( 0 );
			if( Wave->Subtitles.Num() > 0 )
			{
				const FSubtitleCue& Cue = Wave->Subtitles(0);
				Description = FString::Printf( TEXT( "%c \"%s\"" ), Waves.Num() > 1 ? TEXT( '*' ) : TEXT( ' ' ), *Cue.Text );
			}
		}
		break;
	}

	return( Description );
}

/**
 * @return		Sum of the size of waves referenced by this cue.
 */
INT USoundCue::GetResourceSize( void )
{
	if( GExclusiveResourceSizeMode )
	{
		return( 0 );
	}
	else
	{
		// Sum up the size of referenced waves
		FArchiveCountMem CountBytesSize( this );
		INT ResourceSize = CountBytesSize.GetNum();

		TArray<USoundNodeWave*> Waves;
		RecursiveFindNode<USoundNodeWave>( FirstNode, Waves );

		for( INT WaveIndex = 0; WaveIndex < Waves.Num(); ++WaveIndex )
		{
			ResourceSize += Waves( WaveIndex )->GetResourceSize();
		}

		return( ResourceSize );
	}
}

/**
 *	@param		Platform		EPlatformType indicating the platform of interest...
 *
 *	@return		Sum of the size of waves referenced by this cue for the given platform.
 */
INT USoundCue::GetResourceSize( UE3::EPlatformType Platform )
{
	TArray<USoundNodeWave*> Waves;
	RecursiveFindNode<USoundNodeWave>(FirstNode, Waves);

	FArchiveCountMem CountBytesSize(this);
	INT ResourceSize = CountBytesSize.GetNum();
	for (INT WaveIndex = 0; WaveIndex < Waves.Num(); ++WaveIndex)
	{
		ResourceSize += Waves(WaveIndex)->GetResourceSize(Platform);
	}

	return ResourceSize;
}

/** 
 * Standard Serialize function
 */
void USoundCue::Serialize( FArchive& Ar )
{
	Super::Serialize( Ar );

	Ar << EditorData;
	// Empty when loading and we don't care about saving it again, like e.g. a client.
	if( Ar.IsLoading() && ( !GIsEditor && !GIsUCC ) )
	{
		EditorData.Empty();
	}

	if( Ar.Ver() < VER_DEPRECATE_SOUND_RANGES )
	{
		SoundClass = SoundGroup_DEPRECATED;
	}
}

/**
 * Used by various commandlets to purge editor only and platform-specific data from various objects
 * 
 * @param PlatformsToKeep Platforms for which to keep platform-specific data
 * @param bStripLargeEditorData If TRUE, data used in the editor, but large enough to bloat download sizes, will be removed
 */
void USoundCue::StripData(UE3::EPlatformType PlatformsToKeep, UBOOL bStripLargeEditorData)
{
	Super::StripData(PlatformsToKeep, bStripLargeEditorData); 
#if WITH_EDITORONLY_DATA	
	// if we aren't keeping any non-stripped platforms, we can toss the data
	if (!(PlatformsToKeep & ~UE3::PLATFORM_Stripped) || GIsCookingForDemo )
	{
		EditorData.Empty();  // SoundClasses do not do this.
	}
#endif // WITH_EDITORONLY_DATA
}

/**
* Called when a property value from a member struct or array has been changed in the editor.
*/
void USoundCue::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Fixup();
	Super::PostEditChangeProperty(PropertyChangedEvent);
}

/** 
* Populate the enum using the serialised fname
*/
void USoundCue::Fixup( void )
{
	UEnum* SoundClassNamesEnum = FindObject<UEnum>( NULL, TEXT( "Engine.AudioDevice.ESoundClassName" ) );
	if( SoundClassNamesEnum )
	{
		INT SoundClassNameValue = SoundClassNamesEnum->FindEnumIndex( SoundClass );
		if( SoundClassNameValue != INDEX_NONE )
		{
			SoundClassName = ( BYTE )SoundClassNameValue;
		}
		else
		{
			SoundClassName = 0;
			SoundClass = NAME_Master;
		}
	}
}

/*-----------------------------------------------------------------------------
	USoundNode implementation.
-----------------------------------------------------------------------------*/

void USoundNode::PostLoad()
{
	Super::PostLoad();
#if MOBILE
	// detect waves that have their audio stripped due to being on a low memory device and remove them so e.g. random nodes don't select them
	if (GSystemSettings.DetailMode < DM_High)
	{
		INT i = 0;
		while (i < ChildNodes.Num())
		{
			USoundNodeWave* Wave = Cast<USoundNodeWave>(ChildNodes(i));
			if (Wave != NULL && Wave->MobileDetailMode > GSystemSettings.DetailMode)
			{
				RemoveChildNode(i);
			}
			else
			{
				i++;
			}
		}
	}
#endif
}

void USoundNode::ParseNodes( UAudioDevice* AudioDevice, USoundNode* Parent, INT ChildIndex, UAudioComponent* AudioComponent, TArray<FWaveInstance*>& WaveInstances )
{
	for( INT i = 0; i < ChildNodes.Num() && i < GetMaxChildNodes(); i++ )
	{
		if( ChildNodes( i ) )
		{
			ChildNodes( i )->ParseNodes( AudioDevice, this, i, AudioComponent, WaveInstances );
		}
	}
}

void USoundNode::GetNodes( UAudioComponent* AudioComponent, TArray<USoundNode*>& SoundNodes )
{
	SoundNodes.AddItem( this );
	INT MaxChildNodes = GetMaxChildNodes();
	for( INT i = 0; i < ChildNodes.Num() && ( i < MaxChildNodes || MaxChildNodes == -1 ); i++ )
	{
		if( ChildNodes( i ) )
		{
			ChildNodes( i )->GetNodes( AudioComponent, SoundNodes );
		}
	}
}

void USoundNode::GetAllNodes( TArray<USoundNode*>& SoundNodes )
{
	SoundNodes.AddItem( this );
	INT MaxChildNodes = GetMaxChildNodes();
	for( INT i = 0; i < ChildNodes.Num() && ( i < MaxChildNodes || MaxChildNodes == -1 ); i++ )
	{
		if( ChildNodes( i ) )
		{
			ChildNodes( i )->GetAllNodes( SoundNodes );
		}
	}
}

/**
 * Helper function to reset bFinished on wave instances this node has been notified of being finished.
 *
 * @param	AudioComponent	Audio component this node is used in.
 */
void USoundNode::ResetWaveInstances( UAudioComponent* AudioComponent )
{
	// Find all wave instances associated with this node in the passed in audio component.
	TArray<FWaveInstance*> ComponentWaveInstances;
	AudioComponent->SoundNodeResetWaveMap.MultiFind( this, ComponentWaveInstances );

	// Reset bFinished on wave instances found.
	for( INT InstanceIndex = 0; InstanceIndex < ComponentWaveInstances.Num(); InstanceIndex++ )
	{
		FWaveInstance* WaveInstance = ComponentWaveInstances( InstanceIndex );
		WaveInstance->bIsStarted = FALSE;
		WaveInstance->bIsFinished = FALSE;
	}

	// Empty list.
	AudioComponent->SoundNodeResetWaveMap.Remove( this );
}

/**
 * Called by the Sound Cue Editor for nodes which allow children.  The default behaviour is to
 * attach a single connector. Derived classes can override to eg add multiple connectors.
 */
void USoundNode::CreateStartingConnectors( void )
{
	InsertChildNode( ChildNodes.Num() );
}

void USoundNode::InsertChildNode( INT Index )
{
	check( Index >= 0 && Index <= ChildNodes.Num() );
	ChildNodes.InsertZeroed( Index );
}

void USoundNode::RemoveChildNode( INT Index )
{
	check( Index >= 0 && Index < ChildNodes.Num() );
	ChildNodes.Remove( Index );
}

FLOAT USoundNode::GetDuration( void )
{
	// Iterate over children and return maximum length of any of them
	FLOAT MaxDuration = 0.0f;
	for( INT i = 0; i < ChildNodes.Num(); i++ )
	{
		if( ChildNodes( i ) )
		{
			MaxDuration = ::Max( ChildNodes( i )->GetDuration(), MaxDuration );
		}
	}
	return MaxDuration;
}

/** 
 * Check whether to apply the radio filter
 */
UBOOL USoundNode::ApplyRadioFilter( UAudioDevice* AudioDevice, UAudioComponent* AudioComponent )
{
	if( AudioDevice->GetMixDebugState() == DEBUGSTATE_DisableRadio )
	{
		return( FALSE );
	}

	// Make sure the radio filter is requested
	if( AudioComponent->CurrentRadioFilterVolumeThreshold > KINDA_SMALL_NUMBER )
	{
		AudioComponent->bApplyRadioFilter = ( AudioComponent->CurrentVolume < AudioComponent->CurrentRadioFilterVolumeThreshold );
	}

	return( AudioComponent->bApplyRadioFilter );
}

/**
 * Called when a property value from a member struct or array has been changed in the editor.
 */
void USoundNode::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	MarkPackageDirty();
}

/**
 * Calculate the attenuation value.
 * @param DistanceModel - which math model of attenuation is used
 * @param Distance
 * @param UsedMinRadius - if distance is smaller than this radius there is no attenuation
 * @param UsedMaxRadius - if distance is grater than this radius sound is attenuated completely
 *
 * @return Attenuation value (between 0.0 and 1.0)
 */
static FLOAT AttenuationEval(const BYTE DistanceModel, const FLOAT Distance, const FLOAT UsedMinRadius, const FLOAT UsedMaxRadius, const FLOAT dBAttenuationAtMax)
{
	FLOAT Constant;

	if( Distance >= UsedMaxRadius )
	{
		return 0.0f;
	}
	// UsedMinRadius is the point at which to start attenuating
	else if( Distance > UsedMinRadius )
	{
		// determine which AttenuationModel to use here
		if( DistanceModel == ATTENUATION_Linear )
		{
			return ( 1.0f - ( Distance - UsedMinRadius ) / ( UsedMaxRadius - UsedMinRadius ) );
		}
		else if( DistanceModel == ATTENUATION_Logarithmic )
		{
			if( UsedMinRadius == 0.0f )
			{
				Constant = 0.25f;
			}
			else
			{
				Constant = -1.0f / appLoge( UsedMinRadius / UsedMaxRadius );
			}
			return Min( Constant * -appLoge( Distance / UsedMaxRadius ), 1.0f );
		}
		else if( DistanceModel == ATTENUATION_Inverse )
		{
			if( UsedMinRadius == 0.0f )
			{
				Constant = 1.0f;
			}
			else
			{
				Constant = UsedMaxRadius / UsedMinRadius;
			}
			return Min( Constant * ( 0.02f / ( Distance / UsedMaxRadius ) ), 1.0f );
		}
		else if( DistanceModel == ATTENUATION_LogReverse )
		{
			if( UsedMinRadius == 0.0f )
			{
				Constant = 0.25f;
			}
			else
			{
				Constant = -1.0f / appLoge( UsedMinRadius / UsedMaxRadius );
			}
			return Max( 1.0f - Constant * appLoge( 1.0f / ( 1.0f - ( Distance / UsedMaxRadius ) ) ), 0.0f );
		}
		else if( DistanceModel == ATTENUATION_NaturalSound )
		{
			check( dBAttenuationAtMax <= 0.0f );
			FLOAT DistanceInsideMinMax = ( Distance - UsedMinRadius ) / ( UsedMaxRadius - UsedMinRadius );
			return appPow( 10.0f, ( DistanceInsideMinMax * dBAttenuationAtMax ) / 20.0f );
		}
	}
	return 1.0f;
}

/**
 * Calculate the attenuated volume using the available data
 */
void USoundNode::CalculateAttenuatedVolume( UAudioComponent* AudioComponent, const BYTE DistanceModel, const FLOAT Distance, const FLOAT UsedMinRadius, const FLOAT UsedMaxRadius, const FLOAT dBAttenuationAtMax )
{
	AudioComponent->CurrentVolume *= AttenuationEval(DistanceModel, Distance, UsedMinRadius, UsedMaxRadius, dBAttenuationAtMax);
}

/**
 * Calculate the high shelf filter value
 */
void USoundNode::CalculateLPFComponent( UAudioComponent* AudioComponent, const FLOAT Distance, const FLOAT UsedLPFMinRadius, const FLOAT UsedLPFMaxRadius )
{
	if( Distance >= UsedLPFMaxRadius )
	{
		AudioComponent->CurrentHighFrequencyGain = 0.0f;
	}
	// UsedLPFMinRadius is the point at which to start applying the low pass filter
	else if( Distance > UsedLPFMinRadius )
	{
		AudioComponent->CurrentHighFrequencyGain = 1.0f - ( ( Distance - UsedLPFMinRadius ) / ( UsedLPFMaxRadius - UsedLPFMinRadius ) );
	}
	else
	{
		AudioComponent->CurrentHighFrequencyGain = 1.0f;
	}
}

/** 
 * Used to create a unique string to identify unique nodes
 */
FString USoundNode::GetUniqueString( void )
{
	return( FString( "ERROR" ) );
}

IMPLEMENT_CLASS( USoundNode );

/*-----------------------------------------------------------------------------
	USoundNodeMixer implementation.
-----------------------------------------------------------------------------*/

void USoundNodeMixer::ParseNodes( UAudioDevice* AudioDevice, USoundNode* Parent, INT ChildIndex, UAudioComponent* AudioComponent, TArray<FWaveInstance*>& WaveInstances )
{
	// A mixer cannot use seamless looping as it waits for the longest one to finish before it "finishes".
	AudioComponent->CurrentNotifyOnLoop = FALSE;

	for( INT ChildNodeIndex = 0; ChildNodeIndex < ChildNodes.Num(); ChildNodeIndex++ )
	{
		if( ChildNodes( ChildNodeIndex ) )
		{
			FAudioComponentSavedState SavedState;
			SavedState.Set( AudioComponent );

			AudioComponent->CurrentVolume *= InputVolume( ChildNodeIndex );
			ChildNodes( ChildNodeIndex )->ParseNodes( AudioDevice, this, ChildNodeIndex, AudioComponent, WaveInstances );

			SavedState.Restore( AudioComponent );
		}
	}
}

/**
 * Mixers have two connectors by default.
 */
void USoundNodeMixer::CreateStartingConnectors( void )
{
	// Mixers default with two connectors.
	InsertChildNode( ChildNodes.Num() );
	InsertChildNode( ChildNodes.Num() );
}

/**
 * Overloaded to add an entry to InputVolume.
 */
void USoundNodeMixer::InsertChildNode( INT Index )
{
	Super::InsertChildNode( Index );
	InputVolume.Insert( Index );
	InputVolume( Index ) = 1.0f;
}

/**
 * Overloaded to remove an entry from InputVolume.
 */
void USoundNodeMixer::RemoveChildNode( INT Index )
{
	Super::RemoveChildNode( Index );
	InputVolume.Remove( Index );
}

/** 
 * Used to create a unique string to identify unique nodes
 */
FString USoundNodeMixer::GetUniqueString( void )
{
	FString Unique = TEXT( "Mixer" );
	
	for( INT Index = 0; Index < InputVolume.Num(); Index++ )
	{
		Unique += FString::Printf( TEXT( " %g" ), InputVolume( Index ) );
	}

	Unique += TEXT( "/" );
	return( Unique );
}

IMPLEMENT_CLASS( USoundNodeMixer );

/*-----------------------------------------------------------------------------
	USoundNodeDistanceCrossFade implementation.
-----------------------------------------------------------------------------*/

void USoundNodeDistanceCrossFade::Serialize( FArchive& Ar )
{
	Super::Serialize( Ar );
	if( Ar.Ver() < VER_DEPRECATE_SOUND_DISTRIBUTIONS )
	{
		for( INT CrossFadeIndex = 0; CrossFadeIndex < CrossFadeInput.Num(); CrossFadeIndex++ )
		{
			CrossFadeInput( CrossFadeIndex ).FadeInDistance_DEPRECATED.GetOutRange( CrossFadeInput( CrossFadeIndex ).FadeInDistanceStart, CrossFadeInput( CrossFadeIndex ).FadeInDistanceEnd );
			CrossFadeInput( CrossFadeIndex ).FadeOutDistance_DEPRECATED.GetOutRange( CrossFadeInput( CrossFadeIndex ).FadeOutDistanceStart, CrossFadeInput( CrossFadeIndex ).FadeOutDistanceEnd );
		}
	}

#if WITH_EDITOR
	if( ( GIsCooking == TRUE)  && Ar.IsLoading() )
	{
		for( INT CrossFadeIndex = 0; CrossFadeIndex < CrossFadeInput.Num(); CrossFadeIndex++ )
		{
			CrossFadeInput( CrossFadeIndex ).FadeInDistance_DEPRECATED.Distribution = NULL;
			CrossFadeInput( CrossFadeIndex ).FadeOutDistance_DEPRECATED.Distribution = NULL;
		}
	}
#endif
}

FLOAT USoundNodeDistanceCrossFade::MaxAudibleDistance( FLOAT CurrentMaxDistance )
{
	FLOAT Retval = 0.0f;

	for( INT CrossFadeIndex = 0; CrossFadeIndex < CrossFadeInput.Num(); CrossFadeIndex++ )
	{
		FLOAT FadeInDistanceMax = CrossFadeInput( CrossFadeIndex ).FadeInDistanceEnd;
		FLOAT FadeOutDistanceMax = CrossFadeInput( CrossFadeIndex ).FadeOutDistanceEnd;

		if( FadeInDistanceMax > Retval )
		{
			Retval = FadeInDistanceMax;
		}

		if( FadeOutDistanceMax > Retval )
		{
			Retval = FadeOutDistanceMax;
		}
	}

	return( Retval );
}


void USoundNodeDistanceCrossFade::ParseNodes( UAudioDevice* AudioDevice, USoundNode* Parent, INT ChildIndex, UAudioComponent* AudioComponent, TArray<FWaveInstance*>& WaveInstances )
{
	// A "mixer type" cannot use seamless looping as it waits for the longest path to finish before it "finishes".
	AudioComponent->CurrentNotifyOnLoop = FALSE;

	for( INT ChildNodeIndex = 0; ChildNodeIndex < ChildNodes.Num(); ChildNodeIndex++ )
	{
		if( ChildNodes( ChildNodeIndex ) != NULL )
		{
			FAudioComponentSavedState SavedState;
			// save our state for this path through the SoundCue (i.e. at the end we need to remove any modifications parsing the path did so we can parse the next input with a clean slate)
			SavedState.Set( AudioComponent );

			// get the various distances for this input so we can fade in/out the volume correctly
			FLOAT FadeInDistanceMin = CrossFadeInput( ChildNodeIndex ).FadeInDistanceStart;
			FLOAT FadeInDistanceMax = CrossFadeInput( ChildNodeIndex ).FadeInDistanceEnd;

			FLOAT FadeOutDistanceMin = CrossFadeInput( ChildNodeIndex ).FadeOutDistanceStart;
			FLOAT FadeOutDistanceMax = CrossFadeInput( ChildNodeIndex ).FadeOutDistanceEnd;

			// watch out here.  If one is playing the sound on the PlayerController then this will not update correctly as PlayerControllers don't move in normal play
			const FLOAT Distance = FDist( AudioComponent->CurrentLocation, AudioComponent->Listener->Location );

			// determine the volume amount we should set the component to before "playing" 
			FLOAT VolumeToSet = 1.0f;
			//debugf( TEXT("  USoundNodeDistanceCrossFade.  Distance: %f ChildNodeIndex: %d CurrLoc: %s  ListenerLoc: %s"), Distance, ChildNodeIndex, *AudioComponent->CurrentLocation.ToString(), *AudioComponent->Listener->Location.ToString() );

			// Ignore distance calculations for preview components as they are undefined
			if( AudioComponent->bPreviewComponent )
			{
				VolumeToSet = CrossFadeInput( ChildNodeIndex ).Volume;
			}
			else if( ( Distance >= FadeInDistanceMin ) && ( Distance <= FadeInDistanceMax ) )
			{
				VolumeToSet = CrossFadeInput( ChildNodeIndex ).Volume * ( 0.0f + ( Distance - FadeInDistanceMin ) / ( FadeInDistanceMax - FadeInDistanceMin ) );
				//debugf( TEXT("     FadeIn.  Distance: %f,  VolumeToSet: %f"), Distance, VolumeToSet );
			}
			// else if we are inside the FadeOut edge
			else if( ( Distance >= FadeOutDistanceMin ) && ( Distance <= FadeOutDistanceMax ) )
			{
				VolumeToSet = CrossFadeInput( ChildNodeIndex ).Volume * ( 1.0f - ( Distance - FadeOutDistanceMin ) / ( FadeOutDistanceMax - FadeOutDistanceMin ) );
				//debugf( TEXT("     FadeOut.  Distance: %f,  VolumeToSet: %f"), Distance, VolumeToSet );
			}
			// else we are in between the fading edges of the CrossFaded sound and we should play the
			// sound at the CrossFadeInput's specified volume
			else if( ( Distance >= FadeInDistanceMax ) && ( Distance <= FadeOutDistanceMin ) )
			{
				VolumeToSet = CrossFadeInput( ChildNodeIndex ).Volume;
				//debugf( TEXT("     In Between.  Distance: %f,  VolumeToSet: %f"), Distance, VolumeToSet );
			}
			// else we are outside of the range of this CrossFadeInput and should not play anything
			else
			{
				//debugf( TEXT("     OUTSIDE!!!" ));
				VolumeToSet = 0; //CrossFadeInput( ChildNodeIndex ).Volume;
			}

			AudioComponent->CurrentVolume *= VolumeToSet;

			// "play" the rest of the tree
			ChildNodes( ChildNodeIndex )->ParseNodes( AudioDevice, this, ChildNodeIndex, AudioComponent, WaveInstances );

			SavedState.Restore( AudioComponent );
		}
	}
}

/**
 *  USoundNodeDistanceCrossFades have two connectors by default.
 **/
void USoundNodeDistanceCrossFade::CreateStartingConnectors()
{
	// Mixers default with two connectors.
	InsertChildNode( ChildNodes.Num() );
	InsertChildNode( ChildNodes.Num() );
}


void USoundNodeDistanceCrossFade::InsertChildNode( INT Index )
{
	Super::InsertChildNode( Index );
	CrossFadeInput.InsertZeroed( Index );

	CrossFadeInput( Index ).Volume = 1.0f;
}


void USoundNodeDistanceCrossFade::RemoveChildNode( INT Index )
{
	Super::RemoveChildNode( Index );
	CrossFadeInput.Remove( Index );
}

/** 
 * Used to create a unique string to identify unique nodes
 */
FString USoundNodeDistanceCrossFade::GetUniqueString( void )
{
	FString Unique = TEXT( "DistanceCrossFade" );

	Unique += TEXT( "Complex" );

	Unique += TEXT( "/" );
	return( Unique );
}

IMPLEMENT_CLASS( USoundNodeDistanceCrossFade );

/*-----------------------------------------------------------------------------
	USoundNodeWave implementation.
-----------------------------------------------------------------------------*/

/**
 * Returns the size of the object/ resource for display to artists/ LDs in the Editor.
 *
 * @return size of resource as to be displayed to artists/ LDs in the Editor.
 */
INT USoundNodeWave::GetResourceSize( void )
{
	INT ResourceSize = 0;
	
	if (!GExclusiveResourceSizeMode)
	{
		FArchiveCountMem CountBytesSize( this );
		ResourceSize = CountBytesSize.GetNum();
	}

#if _WINDOWS
	if( DecompressionType == DTYPE_Native )
	{
		ResourceSize += RawPCMDataSize; 
	}
	else
	{
		ResourceSize += CompressedPCData.GetBulkDataSize(); 
	}
#elif PS3
	ResourceSize += CompressedPS3Data.GetBulkDataSize(); 
#elif WIIU
	ResourceSize += CompressedWiiUData.GetBulkDataSize();
#elif IPHONE
	ResourceSize += CompressedIPhoneData.GetBulkDataSize();
#elif FLASH
	ResourceSize += CompressedFlashData.GetBulkDataSize();
#elif _XBOX
	UAudioDevice* AudioDevice = GEngine->Client->GetAudioDevice();

	if ( AudioDevice )
	{
		INT BytesUsed = 0;
		UBOOL IsPartOfPool = FALSE;
		if ( ((UXAudio2Device*)AudioDevice)->GetResourceAllocationInfo(ResourceID, /*OUT*/ BytesUsed, /*OUT*/ IsPartOfPool) )
		{
			ResourceSize += (IsPartOfPool && GExclusiveResourceSizeMode) ? 0 : BytesUsed;
		}
		else
		{
			// Best guess
			ResourceSize += CompressedXbox360Data.GetBulkDataSize();
		}
	}
#elif ANDROID || NGP
	if( RawPCMDataSize != 0 )
	{
		ResourceSize += RawPCMDataSize;
	}
	else
	{
		ResourceSize += RawData.GetBulkDataSize();
	}
#endif

	return ResourceSize;
}

/**
 *	@param		Platform		EPlatformType indicating the platform of interest...
 *
 *	@return		Sum of the size of waves referenced by this cue for the given platform.
 */
INT USoundNodeWave::GetResourceSize( UE3::EPlatformType Platform )
{
	FArchiveCountMem CountBytesSize( this );
	INT ResourceSize = CountBytesSize.GetNum();

	if( ( Platform & UE3::PLATFORM_PC ) != 0 )
	{
		if( DecompressionType == DTYPE_Native )
		{
			ResourceSize += RawPCMDataSize; 
		}
		else
		{
			ResourceSize += CompressedPCData.GetBulkDataSize(); 
		}
	}
	else if( ( Platform & UE3::PLATFORM_PS3 ) != 0 )
	{
		ResourceSize += CompressedPS3Data.GetBulkDataSize(); 
	}
	else if( ( Platform & UE3::PLATFORM_Xbox360 ) != 0 )
	{
		ResourceSize += CompressedXbox360Data.GetBulkDataSize();
	}
	else if( ( Platform & UE3::PLATFORM_WiiU ) != 0 )
	{
		ResourceSize += CompressedWiiUData.GetBulkDataSize();
	}
	else if( ( Platform & UE3::PLATFORM_IPhone ) != 0 )
	{
		ResourceSize += CompressedIPhoneData.GetBulkDataSize();
	}
	else if( ( Platform & UE3::PLATFORM_Flash ) != 0 )
	{
		ResourceSize += CompressedFlashData.GetBulkDataSize();
	}

	return ResourceSize;
}

/** 
 * Returns the name of the exporter factory used to export this object
 * Used when multiple factories have the same extension
 */
FName USoundNodeWave::GetExporterName( void )
{
#if WITH_EDITORONLY_DATA
	if( ChannelOffsets.Num() > 0 && ChannelSizes.Num() > 0 )
	{
		return( FName( TEXT( "SoundSurroundExporterWAV" ) ) );
	}
#endif // WITH_EDITORONLY_DATA

	return( FName( TEXT( "SoundExporterWAV" ) ) );
}

/** 
 * Returns a one line description of an object for viewing in the thumbnail view of the generic browser
 */
FString USoundNodeWave::GetDesc( void )
{
	FString Channels;

	if( NumChannels == 0 )
	{
		Channels = TEXT( "Unconverted" );
	}
#if WITH_EDITORONLY_DATA
	else if( ChannelSizes.Num() == 0 )
	{
		Channels = ( NumChannels == 1 ) ? TEXT( "Mono" ) : TEXT( "Stereo" );
	}
#endif // WITH_EDITORONLY_DATA
	else
	{
		Channels = FString::Printf( TEXT( "%d Channels" ), NumChannels );
	}	

	FString Description = FString::Printf( TEXT( "%3.2fs %s" ), GetDuration(), *Channels );
	return Description;
}

/**
 * Returns a detailed description for populating listview columns
 */
FString USoundNodeWave::GetDetailedDescription( INT InIndex )
{
	FString Description = TEXT( "" );
	switch( InIndex )
	{
	case 0:
		if( NumChannels == 0 )
		{
			Description = TEXT( "Unconverted" );
		}
#if WITH_EDITORONLY_DATA
		else if( ChannelSizes.Num() == 0 )
		{
			Description = ( NumChannels == 1 ) ? TEXT( "Mono" ) : TEXT( "Stereo" );
		}
#endif // WITH_EDITORONLY_DATA
		else
		{
			Description = FString::Printf( TEXT( "%d Channels" ), NumChannels );
		}
		break;
	case 1:
		if( SampleRate != 0 )
		{
			Description = FString::Printf( TEXT( "%d Hz" ), SampleRate );
		}
		break;
	case 2:
		Description = FString::Printf( TEXT( "%d pct" ), CompressionQuality );
		break;
	case 3:
		Description = FString::Printf( TEXT( "%2.2f Sec" ), GetDuration() );
		break;
	case 4:
		Description = FString::Printf( TEXT( "%.2f Kb Ogg" ), CompressedPCData.GetBulkDataSize() / 1024.0f );
		break;
	case 5:
		Description = FString::Printf( TEXT( "%.2f Kb XMA" ), CompressedXbox360Data.GetBulkDataSize() / 1024.0f );
		break;
	case 6:
		Description = FString::Printf( TEXT( "%.2f Kb PS3" ), CompressedPS3Data.GetBulkDataSize() / 1024.0f );
		break;
	case 7:
		Description = FString::Printf( TEXT( "%.2f Kb ADPCM" ), CompressedWiiUData.GetBulkDataSize() / 1024.0f );
		break;
	case 8:
		Description = FString::Printf( TEXT( "%.2f Kb IMA4" ), CompressedIPhoneData.GetBulkDataSize() / 1024.0f );
		break;
	case 9:
		Description = FString::Printf( TEXT( "%.2f Kb MP3" ), CompressedFlashData.GetBulkDataSize() / 1024.0f );
		break;
	case 10:
		if( Subtitles.Num() > 0 )
		{
			Description = FString::Printf( TEXT( "\"%s\"" ), *Subtitles( 0 ).Text );
		}
		else
		{
			Description = TEXT( "<no subtitles>" );
		}
		break;
	}
	return( Description );
}

void USoundNodeWave::Serialize( FArchive& Ar )
{
	Super::Serialize( Ar );
	
	RawData.Serialize( Ar, this );
	
	CompressedPCData.Serialize( Ar, this );
	CompressedXbox360Data.Serialize( Ar, this );
	CompressedPS3Data.Serialize( Ar, this );
	if( Ar.Ver() >= VER_WIIU_COMPRESSED_SOUNDS )
	{
		CompressedWiiUData.Serialize( Ar, this );
	}
	if( Ar.Ver() >= VER_IPHONE_COMPRESSED_SOUNDS)
	{
#if MOBILE
		CompressedIPhoneData.Serialize(Ar, this, INDEX_NONE, MobileDetailMode > GSystemSettings.DetailMode);
#else
		CompressedIPhoneData.Serialize(Ar, this);
#endif
	}
	if( Ar.Ver() >= VER_FLASH_MERGE_TO_MAIN)
	{
		CompressedFlashData.Serialize( Ar, this );
	}

	if( Ar.Ver() < VER_MP3ENC_TO_MSENC )
	{
		CompressedPS3Data.RemoveBulkData();
	}

	if( Ar.Ver() < VER_XAUDIO2_FORMAT_UPDATE )
	{
		CompressedXbox360Data.RemoveBulkData();
	}
	if (Ar.Ver() < VER_IPHONE_AUDIO_VARIABLE_BLOCK_SIZE_COMPRESSION)
	{
		CompressedIPhoneData.RemoveBulkData();
	}

	extern UBOOL GForceSoundRecook;
	UBOOL bIsNotUsingAudioAndNeverSaving = !GIsEditor && GIsGame && GEngine && !GEngine->bUseSound;

	if( Ar.IsLoading() )
	{
		if( GForceSoundRecook || bIsNotUsingAudioAndNeverSaving )
		{
			CompressedPCData.RemoveBulkData();
			CompressedPS3Data.RemoveBulkData();
			CompressedXbox360Data.RemoveBulkData();
			CompressedWiiUData.RemoveBulkData();
			CompressedIPhoneData.RemoveBulkData();
			CompressedFlashData.RemoveBulkData();
		}
		if( bIsNotUsingAudioAndNeverSaving )
		{
			RawData.RemoveBulkData();
		}
	}
}

/**
 * Used by various commandlets to purge editor only and platform-specific data from various objects
 * 
 * @param PlatformsToKeep Platforms for which to keep platform-specific data
 * @param bStripLargeEditorData If TRUE, data used in the editor, but large enough to bloat download sizes, will be removed
 */
void USoundNodeWave::StripData(UE3::EPlatformType PlatformsToKeep, UBOOL bStripLargeEditorData)
{
	Super::StripData(PlatformsToKeep, bStripLargeEditorData); 

#if WITH_EDITORONLY_DATA
	// some platforms use the raw data, so only toss the data
	// if we aren't keeping those platforms, unless bStripLargeEditorData is passed in,
	// because the cooker can uncompress the vorbis data - we need to keep one of them 
	// however, so we base it on bStripLargeEditorData
	// note that the cooker can't strip out the uncompressed data when cooking for these
	// platforms because it will have just uncompressed back to the raw data!
	UBOOL bKeepingMobileData = ( PlatformsToKeep & ( UE3::PLATFORM_Android | UE3::PLATFORM_NGP ) ) != 0;
	UBOOL bUseRawPCDataForMobile = bKeepingMobileData && (!bStripLargeEditorData || GIsCooking);
	UBOOL bUseCompressedPCDataForMobile = bKeepingMobileData && !bUseRawPCDataForMobile;

	// do we need to keep compressed data?
	UBOOL bIsCompressedPCDataNeeded = (PlatformsToKeep & (UE3::PLATFORM_Windows | UE3::PLATFORM_WindowsConsole | UE3::PLATFORM_MacOSX)) || bUseCompressedPCDataForMobile;

	// remove cached pre-converted sound data for platforms we aren't keeping
	if (!bIsCompressedPCDataNeeded)
	{
		CompressedPCData.RemoveBulkData();
	}
	if (!(PlatformsToKeep & UE3::PLATFORM_Xbox360))
	{
		CompressedXbox360Data.RemoveBulkData();
	}
	if (!(PlatformsToKeep & UE3::PLATFORM_PS3))
	{
		CompressedPS3Data.RemoveBulkData();
	}
	if (!(PlatformsToKeep & UE3::PLATFORM_WiiU))
	{
		CompressedWiiUData.RemoveBulkData();
	}
	if (!(PlatformsToKeep & UE3::PLATFORM_IPhone))
	{
		CompressedIPhoneData.RemoveBulkData();
	}
	if (!(PlatformsToKeep & UE3::PLATFORM_Flash))
	{
		CompressedFlashData.RemoveBulkData();
	}
	if (!bUseRawPCDataForMobile)
	{
		RawData.RemoveBulkData();
		ChannelOffsets.Empty();
		ChannelSizes.Empty();
	}
#endif // WITH_EDITORONLY_DATA
}

/** 
* Removes the bulk data from any USoundNodeWave objects that were loaded 
*/
void appSoundNodeRemoveBulkData()
{
	for( TObjectIterator<USoundNodeWave> It; It; ++It )
	{
		USoundNodeWave* SoundNodeWave = *It;
		SoundNodeWave->CompressedPS3Data.RemoveBulkData();
		SoundNodeWave->CompressedPCData.RemoveBulkData();
		SoundNodeWave->CompressedXbox360Data.RemoveBulkData();
		SoundNodeWave->CompressedWiiUData.RemoveBulkData();
		SoundNodeWave->CompressedIPhoneData.RemoveBulkData();
		SoundNodeWave->CompressedFlashData.RemoveBulkData();
	}
}

/**
 * Outside the Editor, uploads resource to audio device and performs general PostLoad work.
 *
 * This function is being called after all objects referenced by this object have been serialized.
 */
extern INT Localization_GetLanguageExtensionIndex( const TCHAR* Ext );
/** 
 *	For now, we will assume *all* subtitles get packed w/ the same languages.
 *	So on the first load, we will find the correct loc'd subtitle and set this 
 *	index for subsequency postload calls to utilize.
 */
static INT GLanguageExtensionIndex = -1;
static INT GINTLanguageIndex = -1;

void USoundNodeWave::PostLoad( void )
{
	Super::PostLoad();

	// Only the cooked version will have the LocalizedSubtitles array filled in...
	if( LocalizedSubtitles.Num() > 0 )
	{
		if (GLanguageExtensionIndex == -1)
		{
			debugfSlow(NAME_Log, TEXT("*** Dumping Known Language Extensions:"));
			const TArray<FString>& KnownLanguageExtensions = appGetKnownLanguageExtensions();
			for (INT DumpIdx = 0; DumpIdx < KnownLanguageExtensions.Num(); DumpIdx++)
			{
				debugfSlow(NAME_Log, TEXT("\t%2d: %s"), DumpIdx, *(KnownLanguageExtensions(DumpIdx)));
			}

			debugfSlow(NAME_Log, TEXT("*** Determining the current language index..."));
			INT INTLanguageIndex = -1;
			for (INT FindExtIdx = 0; FindExtIdx < LocalizedSubtitles.Num(); FindExtIdx++)
			{
				FLocalizedSubtitle& LocdSub = LocalizedSubtitles(FindExtIdx);
				if (LocdSub.LanguageExt == TEXT("INT"))
				{
					// Assumption: INT will *always* be present...
					GINTLanguageIndex = FindExtIdx;
					debugfSlow(NAME_Warning, TEXT("\tFound INT at index %d"), GINTLanguageIndex);
				}
				if (LocdSub.LanguageExt == UObject::GetLanguage())
				{
					GLanguageExtensionIndex = FindExtIdx;
					debugfSlow(NAME_Log, TEXT("\tFound language %s at index %2d!"), UObject::GetLanguage(), GLanguageExtensionIndex);
				}
			}

			if (GLanguageExtensionIndex == -1)
			{
				debugfSlow(NAME_Warning, TEXT("\tDid not find language extension %s"), UObject::GetLanguage());
				if (GINTLanguageIndex != -1)
				{
					debugfSlow(NAME_Warning, TEXT("\t\tDefaulting to INT at index %d"), GINTLanguageIndex);
					GLanguageExtensionIndex = GINTLanguageIndex;
				}
				else
				{
					FLocalizedSubtitle& LocdSub = LocalizedSubtitles(0);
					debugfSlow(NAME_Warning, TEXT("\t\tINT also not found... Defaulting to %s"), *(LocdSub.LanguageExt));
					GLanguageExtensionIndex = 0;
				}
			}
		}

		// Copy the proper one into the 'real' properties, then clear the array.
		FLocalizedSubtitle& LocSubtitle = LocalizedSubtitles(GLanguageExtensionIndex);
		// Copy it into the appropriate slot
		bManualWordWrap = LocSubtitle.bManualWordWrap;
		bMature = LocSubtitle.bMature;
		bSingleLine = LocSubtitle.bSingleLine;
		Subtitles = LocSubtitle.Subtitles;

		// Clear the array - no sense storing what we don't need.
		LocalizedSubtitles.Empty();

		// Only INT has mature lines
		if ((GLanguageExtensionIndex != GINTLanguageIndex)
			&& (GINTLanguageIndex >= 0) // Safety catch for when GINTLanguageIndex hasn't been set...
			)
		{
			bMature = FALSE;
		}
	}

	// We don't precache default objects and we don't precache in the Editor as the latter will
	// most likely cause us to run out of memory.
	if( !GIsEditor && !IsTemplate( RF_ClassDefaultObject ) && GEngine && GEngine->Client )
	{
		UAudioDevice* AudioDevice = GEngine->Client->GetAudioDevice();
		if( AudioDevice )
		{
#if WITH_TTS
			// Create the TTS PCM data if necessary
			if( bUseTTS && ( ResourceID == 0 ) && ( RawPCMData == NULL ) )
			{
				AudioDevice->TextToSpeech->CreatePCMData( this );
			}
#endif // WITH_TTS
			// Upload the data to the hardware
			AudioDevice->Precache( this );
		}
		// remove bulk data if no AudioDevice is used and no sounds were initialized
		else if( GIsGame )
		{
			RawData.RemoveBulkData();
			CompressedPS3Data.RemoveBulkData();
			CompressedPCData.RemoveBulkData();
			CompressedXbox360Data.RemoveBulkData();
			CompressedWiiUData.RemoveBulkData();
			CompressedIPhoneData.RemoveBulkData();
			CompressedFlashData.RemoveBulkData();
		}
	}

	INC_FLOAT_STAT_BY( STAT_AudioBufferTime, Duration );
	INC_FLOAT_STAT_BY( STAT_AudioBufferTimeChannels, NumChannels * Duration );
}

/** 
 * Copy the compressed audio data from the bulk data
 */
void USoundNodeWave::InitAudioResource( FByteBulkData& CompressedData )
{
	if( !ResourceSize )
	{
		// Grab the compressed vorbis data from the bulk data
		ResourceSize = CompressedData.GetBulkDataSize();
		if( ResourceSize > 0 )
		{
			CompressedData.GetCopy( ( void** )&ResourceData, TRUE );
		}
	}
}

/** 
 * Remove the compressed audio data associated with the passed in wave
 */
void USoundNodeWave::RemoveAudioResource( void )
{
	if( ResourceData )
	{
		appFree( ( void* )ResourceData );
		ResourceSize = 0;
		ResourceData = NULL;
	}
}

/**
 * Returns whether this wave file is a localized resource.
 *
 * @return TRUE if it is a localized resource, FALSE otherwise.
 */
UBOOL USoundNodeWave::IsLocalizedResource( void )
{
	FString FullPathName;
	UBOOL bIsLocalised = FALSE;

	if( GPackageFileCache->FindPackageFile( *GetOutermost()->GetPathName(), NULL, FullPathName ) )
	{
		FString BasePath = TEXT( "\\Sounds\\" );

		// Package is localised if it resides in "\Sounds\XXX" (where XXX = 3 letter language code)
		INT Offset = FullPathName.InStr( BasePath, FALSE, TRUE );
		if( Offset >= 0 )
		{
			// Extract the language folder
			FString LangPath = FullPathName.Mid( Offset + BasePath.Len(), 4 );
			// Ensure it is 3 characters
			if( LangPath[3] == '\\' )
			{
				LangPath[3] = 0;
				// Ensure it is a valid language
				if( Localization_GetLanguageExtensionIndex( *LangPath ) >= 0 )
				{
					bIsLocalised = TRUE;
				}
			}
		}
	}

	return( Super::IsLocalizedResource() || Subtitles.Num() > 0 || bIsLocalised );
}

/** 
 * Invalidate compressed data
 */
void USoundNodeWave::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{	
	UProperty* PropertyThatChanged = PropertyChangedEvent.Property;
#if _WINDOWS
	// Clear out the generated PCM data if any of the TTS fields are changed
	if( PropertyThatChanged == NULL
		|| PropertyThatChanged->GetFName() == SpokenTextFName
		|| PropertyThatChanged->GetFName() == TTSSpeakerFName
		|| PropertyThatChanged->GetFName() == bUseTTSFName )
	{
		UAudioDevice* AudioDevice = NULL;

		// Stop all sounds in case any SoundNodeWaves are actively being played
		if( GEngine && GEngine->Client )
		{
			AudioDevice = GEngine->Client->GetAudioDevice();
			if( AudioDevice )
			{
				AudioDevice->StopAllSounds( TRUE );
			}
		}

		// Free up all old resources
		FreeResources();

		// Generate new resources
		if( AudioDevice )
		{
			if( bUseTTS )
			{
#if WITH_TTS
				AudioDevice->TextToSpeech->CreatePCMData( this );
#endif // WITH_TTS
			}
			else
			{
#if WITH_EDITOR
				ValidateData();
#endif
			}
		
			MarkPackageDirty();
		}
	}

	// Regenerate on save any compressed sound formats
	if( PropertyThatChanged && 
		( PropertyThatChanged->GetFName() == CompressionQualityFName 
		|| PropertyThatChanged->GetFName() == ForceRealTimeDecompressionFName
		|| PropertyThatChanged->GetFName() == LoopingSoundFName
		) )
	{
		CompressedPCData.RemoveBulkData();
		CompressedXbox360Data.RemoveBulkData();
		CompressedPS3Data.RemoveBulkData();
		CompressedWiiUData.RemoveBulkData();
		CompressedIPhoneData.RemoveBulkData();
		CompressedFlashData.RemoveBulkData();

		NumChannels = 0;

		MarkPackageDirty();
	}
#endif
}

/** 
 * Frees up all the resources allocated in this class
 */
void USoundNodeWave::FreeResources( void )
{
	// Housekeeping of stats
	DEC_FLOAT_STAT_BY( STAT_AudioBufferTime, Duration );
	DEC_FLOAT_STAT_BY( STAT_AudioBufferTimeChannels, NumChannels * Duration );

	// GEngine is NULL during script compilation and GEngine->Client and its audio device might be
	// destroyed first during the exit purge.
	if( GEngine && !GExitPurge )
	{
		// Notify the audio device to free the bulk data associated with this wave.
		UAudioDevice* AudioDevice = GEngine->Client ? GEngine->Client->GetAudioDevice() : NULL;
		if( AudioDevice )
		{
			AudioDevice->FreeResource( this );
		}
	}

	NumChannels = 0;
	SampleRate = 0;
	Duration = 0.0f;
	ResourceID = 0;
	bDynamicResource = FALSE;
}

/**
 * Returns whether the resource is ready to have finish destroy routed.
 *
 * @return	TRUE if ready for deletion, FALSE otherwise.
 */
UBOOL USoundNodeWave::IsReadyForFinishDestroy( void )
{
	// Wait till vorbis decompression finishes before deleting resource.
	return( ( VorbisDecompressor == NULL ) || VorbisDecompressor->IsDone() );
}

/**
 * Frees the sound resource data.
 */
void USoundNodeWave::FinishDestroy( void )
{
	FreeResources();

	Super::FinishDestroy();
}

/** 
 * Find an existing waveinstance attached to this audio component (if any)
 */
FWaveInstance* USoundNodeWave::FindWaveInstance( UAudioComponent* AudioComponent, QWORD ParentGUID )
{
	for( INT WaveIndex = 0; WaveIndex < AudioComponent->WaveInstances.Num(); WaveIndex++ )
	{
		FWaveInstance* ExistingWaveInstance = AudioComponent->WaveInstances( WaveIndex );
		if( ExistingWaveInstance->WaveData == this && ExistingWaveInstance->ParentGUID == ParentGUID )
		{
			return( ExistingWaveInstance );
		}
	}

	return( NULL );
}

/** 
 * Handle any special requirements when the sound starts (e.g. subtitles)
 */
FWaveInstance* USoundNodeWave::HandleStart( UAudioComponent* AudioComponent, QWORD ParentGUID )
{
	// Create a new wave instance and associate with the audiocomponent
	FWaveInstance* WaveInstance = new FWaveInstance( AudioComponent );
	WaveInstance->ParentGUID = ParentGUID;
	AudioComponent->WaveInstances.AddItem( WaveInstance );

	// Add in the subtitle if they exist
	const UBOOL bInterceptSubtitles = OBJ_DELEGATE_IS_SET( AudioComponent, OnQueueSubtitles );
	if( !AudioComponent->bSuppressSubtitles || bInterceptSubtitles )
	{
		// Subtitles are hashed based on the associated sound (wave instance).
		if( Subtitles.Num() > 0 )
		{
			if( bInterceptSubtitles )
			{
				// intercept the subtitles if the delegate is set
				AudioComponent->delegateOnQueueSubtitles( Subtitles, Duration );
			}
			else
			{
				// otherwise, pass them on to the subtitle manager for display
				FSubtitleManager::GetSubtitleManager()->QueueSubtitles( ( PTRINT )WaveInstance, AudioComponent->SubtitlePriority, bManualWordWrap, bSingleLine, Duration, Subtitles );
			}
		}
	}

	return( WaveInstance );
}

/** 
 * Handle the terminating nodes of the sound cue 
 */
void USoundNodeWave::ParseNodes( UAudioDevice* AudioDevice, USoundNode* Parent, INT ChildIndex, UAudioComponent* AudioComponent, TArray<FWaveInstance*>& WaveInstances )
{
	AudioComponent->CurrentVolume *= Volume;
	AudioComponent->CurrentPitch *= Pitch;

	// See whether this SoundNodeWave already has a WaveInstance associated with it. Note that we also have to handle
	// the same SoundNodeWave being used multiple times inside the SoundCue which is why we're using a GUID.
	QWORD ParentGUID = ( ( QWORD )( ( Parent ? Parent->GetIndex() : 0xFFFFFFFF ) ) << 32 ) | ( ( DWORD )ChildIndex );
	FWaveInstance* WaveInstance = FindWaveInstance( AudioComponent, ParentGUID );

#if WITH_TTS
	// Expand the TTS data if required
	if( bUseTTS && ( ResourceID == 0 ) && ( RawPCMData == NULL ) )
	{
		AudioDevice->TextToSpeech->CreatePCMData( this );
	}
#endif // WITH_TTS

	// Create a new WaveInstance if this SoundNodeWave doesn't already have one associated with it.
	if( WaveInstance == NULL )
	{
		if( !AudioComponent->bRadioFilterSelected )
		{
			ApplyRadioFilter( AudioDevice, AudioComponent );
			AudioComponent->bRadioFilterSelected = TRUE;
		}

		WaveInstance = HandleStart( AudioComponent, ParentGUID );
	}

	// Check for finished paths.
	if( !WaveInstance->bIsFinished )
	{
		// Propagate properties and add WaveInstance to outgoing array of FWaveInstances.
		WaveInstance->Volume = AudioComponent->CurrentVolume;
		WaveInstance->VolumeMultiplier = AudioComponent->CurrentVolumeMultiplier;
		WaveInstance->PlayPriority = AudioComponent->CurrentVolume + ( AudioComponent->bAlwaysPlay ? 1.0f : 0.0f ) + AudioComponent->CurrentRadioFilterVolume;
		WaveInstance->Pitch = AudioComponent->CurrentPitch * AudioComponent->CurrentPitchMultiplier;
		WaveInstance->HighFrequencyGain = AudioComponent->CurrentHighFrequencyGain * AudioComponent->CurrentHighFrequencyGainMultiplier;
		WaveInstance->VoiceCenterChannelVolume = AudioComponent->CurrentVoiceCenterChannelVolume;
		WaveInstance->RadioFilterVolume = AudioComponent->CurrentRadioFilterVolume;
		WaveInstance->RadioFilterVolumeThreshold = AudioComponent->CurrentRadioFilterVolumeThreshold;
		WaveInstance->bApplyRadioFilter = AudioComponent->bApplyRadioFilter;
		WaveInstance->OmniRadius = AudioComponent->OmniRadius;

		// Properties from the sound class
		WaveInstance->StereoBleed = AudioComponent->StereoBleed;
		WaveInstance->LFEBleed = AudioComponent->LFEBleed;
		WaveInstance->bEQFilterApplied = AudioComponent->bEQFilterApplied;
		WaveInstance->bAlwaysPlay = AudioComponent->bAlwaysPlay;
		WaveInstance->bIsUISound = AudioComponent->bIsUISound;
		WaveInstance->bIsMusic = AudioComponent->bIsMusic;
		WaveInstance->bReverb = AudioComponent->bReverb;
		WaveInstance->bCenterChannelOnly = AudioComponent->bCenterChannelOnly;

		WaveInstance->Location = AudioComponent->CurrentLocation;
		WaveInstance->bIsStarted = TRUE;
		WaveInstance->bAlreadyNotifiedHook = FALSE;
		WaveInstance->bUseSpatialization = AudioComponent->CurrentUseSpatialization;
		WaveInstance->WaveData = this;
		WaveInstance->NotifyBufferFinishedHook = AudioComponent->CurrentNotifyBufferFinishedHook;

		WaveInstance->LoopingMode = LOOP_Never;
		if( AudioComponent->CurrentNotifyOnLoop )
		{
			WaveInstance->LoopingMode = LOOP_WithNotification;
		}

		// Don't add wave instances that are not going to be played at this point.
		if( WaveInstance->PlayPriority > KINDA_SMALL_NUMBER )
		{
			WaveInstances.AddItem( WaveInstance );
		}

		// We're still alive.
		AudioComponent->bFinished = FALSE;

		// Sanity check
		if( NumChannels > 2 && WaveInstance->bUseSpatialization )
		{
			debugf( NAME_Warning, TEXT( "Spatialisation on stereo and multichannel sounds is not supported (%s)" ), *GetName() );
		}
	}
}

FLOAT USoundNodeWave::GetDuration( void )
{
	// Just return the duration of this sound data.
	return( Duration );
}

/** 
 * Used to create a unique string to identify unique nodes
 */
FString USoundNodeWave::GetUniqueString( void )
{
	FString Unique = TEXT( "" );
	return( Unique );
}

IMPLEMENT_CLASS( USoundNodeWave );

/*-----------------------------------------------------------------------------
	USoundNodeWaveStreaming implementation.
-----------------------------------------------------------------------------*/

void USoundNodeWaveStreaming::QueueAudio( const TArray<BYTE>& Data )
{
	check( ( Data.Num() % sizeof( SWORD ) ) == 0 );
	const INT Position = QueuedAudio.Add( Data.Num() );
	appMemcpy( &QueuedAudio( Position ), &Data( 0 ), Data.Num() );
}

void USoundNodeWaveStreaming::QueueSilence(FLOAT Seconds)
{
	if (Seconds > 0.0)
	{
		INT Count = (SampleRate * sizeof(SWORD)) * Seconds;

		while ((Count % sizeof(SWORD)) != 0)
		{
			Count++;
		}

		QueuedAudio.AddZeroed(Count);
	}
}

void USoundNodeWaveStreaming::ResetAudio( void )
{
	QueuedAudio.Empty();
}

INT USoundNodeWaveStreaming::AvailableAudioBytes( void )
{
	return QueuedAudio.Num();
}

void USoundNodeWaveStreaming::GeneratePCMData( TArray<BYTE>& Buffer, INT SamplesNeeded )
{
	const INT SamplesAvailable = QueuedAudio.Num() / sizeof( SWORD );
	const INT SamplesToCopy = Min<INT>( SamplesNeeded, SamplesAvailable );
	const INT BytesToCopy = SamplesToCopy * sizeof( SWORD );
	const INT Position = Buffer.Add( BytesToCopy );
	appMemcpy( &Buffer( Position ), &QueuedAudio( 0 ), BytesToCopy );
	QueuedAudio.Remove( 0, BytesToCopy );
}

/** 
 * Used to create a unique string to identify unique nodes
 */
FString USoundNodeWaveStreaming::GetUniqueString( void )
{
	FString Unique = TEXT( "WaveStreaming" );

	Unique += TEXT( "/" );
	return( Unique );
}

IMPLEMENT_CLASS( USoundNodeWaveStreaming );

/*-----------------------------------------------------------------------------
	USoundNodeAttenuation implementation.
-----------------------------------------------------------------------------*/

/** 
 *  Calculate distance between AudioComponent->CurrentLocation and AudioComponent->Listener->Location using given metric space.
 *  @param DistanceType - metric space
 *  @param AudioComponent
 *  @return calculated distance
 */
static FLOAT EvalDistance(ESoundDistanceCalc DistanceType,  UAudioComponent* AudioComponent)
	{
		FLOAT Distance = 0.0f;
	const FVector PointForDistanceEval = AudioComponent->GetPointForDistanceEval();

	if(NULL != AudioComponent)
	{
		switch( DistanceType )
		{
		case SOUNDDISTANCE_Normal:
		default:
			Distance = FDist( PointForDistanceEval, AudioComponent->Listener->Location );
			break;

		case SOUNDDISTANCE_InfiniteXYPlane:
			Distance = fabsf( PointForDistanceEval.Z - AudioComponent->Listener->Location.Z );
			break;

		case SOUNDDISTANCE_InfiniteXZPlane:
			Distance = fabsf( PointForDistanceEval.Y - AudioComponent->Listener->Location.Y );
			break;

		case SOUNDDISTANCE_InfiniteYZPlane:
			Distance = fabsf( PointForDistanceEval.X - AudioComponent->Listener->Location.X );
			break;
		}
	}
	return Distance;
}

void USoundNodeAttenuation::ParseNodes( UAudioDevice* AudioDevice, USoundNode* Parent, INT ChildIndex, UAudioComponent* AudioComponent, TArray<FWaveInstance*>& WaveInstances )
{
	if( AudioComponent->bAllowSpatialization )
	{
		// Calculate distance from source to listener
		const FLOAT Distance = EvalDistance((ESoundDistanceCalc)DistanceType, AudioComponent);

		// Attenuate the volume based on the model
		if( bAttenuate )
		{
			CalculateAttenuatedVolume( AudioComponent, DistanceAlgorithm, Distance, RadiusMin, RadiusMax, dBAttenuationAtMax );
		}

		// Attenuate with the low pass filter if necessary
		if( bAttenuateWithLPF )
		{
			CalculateLPFComponent( AudioComponent, Distance, LPFRadiusMin, LPFRadiusMax );
		}

		AudioComponent->CurrentUseSpatialization |= bSpatialize;
		AudioComponent->OmniRadius = OmniRadius;
	}
	else
	{
		AudioComponent->CurrentUseSpatialization = FALSE;
	}

	Super::ParseNodes( AudioDevice, Parent, ChildIndex, AudioComponent, WaveInstances );
}

/** 
 * Used to create a unique string to identify unique nodes
 */
FString USoundNodeAttenuation::GetUniqueString( void )
{
	FString Unique = TEXT( "Attenuation" );

	if( bAttenuate )
	{
		Unique += FString::Printf( TEXT( " Vol %g %g" ), RadiusMin, RadiusMax );
	}
	if( bAttenuateWithLPF )
	{
		Unique += FString::Printf( TEXT( " Lpf %g %g" ), LPFRadiusMin, LPFRadiusMax );
	}
	
	Unique += bSpatialize ? TEXT( " Spat" ) : TEXT( "" );
	Unique += FString::Printf( TEXT( " %d %d %g" ), DistanceAlgorithm, DistanceType, dBAttenuationAtMax );

	Unique += TEXT( "/" );
	return( Unique );
}

IMPLEMENT_CLASS( USoundNodeAttenuation );

/*-----------------------------------------------------------------------------
	USoundNodeLooping implementation.
-----------------------------------------------------------------------------*/

// Value used to indicate that loop has finished.
#define LOOP_FINISHED 0

void USoundNodeLooping::Serialize( FArchive& Ar )
{
	Super::Serialize( Ar );
	if( Ar.Ver() < VER_DEPRECATE_SOUND_DISTRIBUTIONS )
	{
		LoopCount_DEPRECATED.GetOutRange( LoopCountMin, LoopCountMax );
	}

#if WITH_EDITOR
	if( ( GIsCooking == TRUE ) && Ar.IsLoading() )
	{
		LoopCount_DEPRECATED.Distribution = NULL;
	}
#endif
}

void USoundNodeLooping::ParseNodes( UAudioDevice* AudioDevice, USoundNode* Parent, INT ChildIndex, UAudioComponent* AudioComponent, TArray<FWaveInstance*>& WaveInstances )
{
	RETRIEVE_SOUNDNODE_PAYLOAD( sizeof( INT ) + sizeof( INT ) );
	DECLARE_SOUNDNODE_ELEMENT( INT, UsedLoopCount );
	DECLARE_SOUNDNODE_ELEMENT( INT, FinishedCount );
		
	if( *RequiresInitialization )
	{
		UsedLoopCount = appTrunc( LoopCountMax + ( ( LoopCountMin - LoopCountMax ) * appSRand() ) );
		FinishedCount = 0;

		*RequiresInitialization = 0;
	}

	if( bLoopIndefinitely || UsedLoopCount > LOOP_FINISHED )
	{
		AudioComponent->CurrentNotifyBufferFinishedHook = this;
		AudioComponent->CurrentNotifyOnLoop = TRUE;
	}
	
	Super::ParseNodes( AudioDevice, Parent, ChildIndex, AudioComponent, WaveInstances );
}

/**
 * Returns whether the node is finished after having been notified of buffer being finished.
 *
 * @param	AudioComponent	Audio component containing payload data
 * @return	TRUE if finished, FALSE otherwise.
 */
UBOOL USoundNodeLooping::IsFinished( UAudioComponent* AudioComponent ) 
{ 
	RETRIEVE_SOUNDNODE_PAYLOAD( sizeof( INT ) + sizeof( INT ) );
	DECLARE_SOUNDNODE_ELEMENT( INT, UsedLoopCount );
	DECLARE_SOUNDNODE_ELEMENT( INT, FinishedCount );

	check( *RequiresInitialization == 0 );

	// Sounds that loop indefinitely can never be finished
	if( bLoopIndefinitely )
	{
		return( FALSE );
	}

	// The -1 is a bit unintuitive as a loop count of 1 means playing the sound twice
	// so NotifyWaveInstanceFinished checks for it being >= 0 as a value of 0 means
	// that we should play it once more.
	return( UsedLoopCount == LOOP_FINISHED );  
}

/**
 * Notifies the sound node that a wave instance in its subtree has finished.
 *
 * @param WaveInstance	WaveInstance that was finished 
 */
UBOOL USoundNodeLooping::NotifyWaveInstanceFinished( FWaveInstance* WaveInstance )
{
	UAudioComponent* AudioComponent = WaveInstance->AudioComponent;
	RETRIEVE_SOUNDNODE_PAYLOAD( sizeof( INT ) + sizeof( INT ) );
	DECLARE_SOUNDNODE_ELEMENT( INT, UsedLoopCount );
	DECLARE_SOUNDNODE_ELEMENT( INT, FinishedCount );

	check( *RequiresInitialization == 0 );
	
	//debugf( TEXT( "NotifyWaveInstanceFinished: %d" ), UsedLoopCount );

	// Maintain loop count
	if( bLoopIndefinitely || UsedLoopCount > LOOP_FINISHED )
	{
		++FinishedCount;

		// Add to map of nodes that might need to have bFinished reset.
		AudioComponent->SoundNodeResetWaveMap.AddUnique( this, WaveInstance );

		// Retrieve all child nodes.
		TArray<USoundNode*> AllChildNodes;
		GetAllNodes( AllChildNodes );

		// Determine if all of the leaf sound node waves have finished playing
		UBOOL bAllLeavesFinished = TRUE;
		for( INT InstanceIndex = 0; InstanceIndex < AudioComponent->WaveInstances.Num() && bAllLeavesFinished; ++InstanceIndex )
		{
			const FWaveInstance* ComponentWaveInstance = AudioComponent->WaveInstances( InstanceIndex );
			
			// See if the current wave instance is represented by a sound node wave in the looping node's subtree
			const UBOOL bInSubtree = AllChildNodes.ContainsItem( ComponentWaveInstance->WaveData );

			// If the wave instance is in the subtree but hasn't finished playing yet, then not all leaves have finished!
			if( bInSubtree && ComponentWaveInstance->bIsStarted && !ComponentWaveInstance->bIsFinished ) 
			{
				bAllLeavesFinished = FALSE;
			}
		}

		// Wait till all leaves are finished.
		if( bAllLeavesFinished )
		{
			FinishedCount = 0;

			// Only decrease loop count if all leaves are finished.
			UsedLoopCount--;

			// GetAllNodes includes current node so we have to start at Index 1.
			for( INT NodeIndex = 1; NodeIndex < AllChildNodes.Num(); NodeIndex++ )
			{
				// Reset all child nodes so they are initialized again.
				USoundNode* ChildNode = AllChildNodes( NodeIndex );
				UINT* Offset = AudioComponent->SoundNodeOffsetMap.Find( ChildNode );
				if( Offset )
				{
					UBOOL* bRequiresInitialization = ( UBOOL* )&AudioComponent->SoundNodeData( *Offset );
					*bRequiresInitialization = TRUE;
				}
			}
		
			// Reset wave instances that notified us of completion.
			ResetWaveInstances( AudioComponent );
			return( UsedLoopCount == LOOP_FINISHED );
		}
	}

	return( FALSE );
}

/** 
 * Returns whether the sound is looping indefinitely or not.
 */
UBOOL USoundNodeLooping::IsLoopingIndefinitely( void )
{
	return( bLoopIndefinitely );
}

/** 
 * Returns the maximum duration of the owning sound cue correctly accounting for loop count
 */
FLOAT USoundNodeLooping::GetDuration( void )
{
	// Sounds that loop forever return a long time
	if( bLoopIndefinitely )
	{
		return( INDEFINITELY_LOOPING_DURATION );
	}

	// Get length of child node
	FLOAT ChildDuration = 0.0f;
	if( ChildNodes( 0 ) )
	{
		ChildDuration = ChildNodes( 0 )->GetDuration();
	}

	// Return these multiplied together after taking into account that a loop count of 0 means it still is 
	// being played at least once.
	return( ChildDuration * ( LoopCountMax + 1 ) );
}

/** 
 * Used to create a unique string to identify unique nodes
 */
FString USoundNodeLooping::GetUniqueString( void )
{
	FString Unique = TEXT( "Looping" );

	if( bLoopIndefinitely )
	{
		Unique += TEXT( " Forever" );
	}
	else
	{
		Unique += FString::Printf( TEXT( " %g %g" ), LoopCountMin, LoopCountMax );
	}

	Unique += TEXT( "/" );
	return( Unique );
}

IMPLEMENT_CLASS( USoundNodeLooping );

/*-----------------------------------------------------------------------------
	USoundNodeOscillator implementation.
-----------------------------------------------------------------------------*/

void USoundNodeOscillator::Serialize( FArchive& Ar )
{
	Super::Serialize( Ar );
	if( Ar.Ver() < VER_DEPRECATE_SOUND_DISTRIBUTIONS )
	{
		Amplitude_DEPRECATED.GetOutRange( AmplitudeMin, AmplitudeMax );
		Frequency_DEPRECATED.GetOutRange( FrequencyMin, FrequencyMax );
		Offset_DEPRECATED.GetOutRange( OffsetMin, OffsetMax );
		Center_DEPRECATED.GetOutRange( CenterMin, CenterMax );
	}

#if WITH_EDITOR
	if( ( GIsCooking == TRUE ) && Ar.IsLoading() )
	{
		Amplitude_DEPRECATED.Distribution = NULL;
		Frequency_DEPRECATED.Distribution = NULL;
		Offset_DEPRECATED.Distribution = NULL;
		Center_DEPRECATED.Distribution = NULL;
	}
#endif
}

void USoundNodeOscillator::ParseNodes( UAudioDevice* AudioDevice, USoundNode* Parent, INT ChildIndex, UAudioComponent* AudioComponent, TArray<FWaveInstance*>& WaveInstances )
{
	RETRIEVE_SOUNDNODE_PAYLOAD( sizeof( FLOAT ) + sizeof( FLOAT ) + sizeof( FLOAT ) + sizeof( FLOAT ) );
	DECLARE_SOUNDNODE_ELEMENT( FLOAT, UsedAmplitude );
	DECLARE_SOUNDNODE_ELEMENT( FLOAT, UsedFrequency );
	DECLARE_SOUNDNODE_ELEMENT( FLOAT, UsedOffset );
	DECLARE_SOUNDNODE_ELEMENT( FLOAT, UsedCenter );

	if( *RequiresInitialization )
	{
		UsedAmplitude = AmplitudeMax + ( ( AmplitudeMin - AmplitudeMax ) * appSRand() );
		UsedFrequency = FrequencyMax + ( ( FrequencyMin - FrequencyMax ) * appSRand() );
		UsedOffset = OffsetMax + ( ( OffsetMin - OffsetMax ) * appSRand() );
		UsedCenter = CenterMax + ( ( CenterMin - CenterMax ) * appSRand() );

		*RequiresInitialization = 0;
	}

	const FLOAT ModulationFactor = UsedCenter + UsedAmplitude * appSin( UsedOffset + UsedFrequency * AudioComponent->PlaybackTime * PI );
	if( bModulateVolume )
	{
		AudioComponent->CurrentVolume *= ModulationFactor;
	}

	if( bModulatePitch )
	{
		AudioComponent->CurrentPitch *= ModulationFactor;
	}

	Super::ParseNodes( AudioDevice, Parent, ChildIndex, AudioComponent, WaveInstances );
}

/** 
 * Used to create a unique string to identify unique nodes
 */
FString USoundNodeOscillator::GetUniqueString( void )
{
	FString Unique = TEXT( "Oscillator" );

	Unique += TEXT( "Complex" );

	Unique += TEXT( "/" );
	return( Unique );
}

IMPLEMENT_CLASS( USoundNodeOscillator );

/*-----------------------------------------------------------------------------
	USoundNodeModulator implementation.
-----------------------------------------------------------------------------*/

void USoundNodeModulator::Serialize( FArchive& Ar )
{
	Super::Serialize( Ar );
	if( Ar.Ver() < VER_DEPRECATE_SOUND_DISTRIBUTIONS )
	{
		VolumeModulation_DEPRECATED.GetOutRange( VolumeMin, VolumeMax );
		PitchModulation_DEPRECATED.GetOutRange( PitchMin, PitchMax );
	}

#if WITH_EDITOR
	if( ( GIsCooking == TRUE ) && Ar.IsLoading() )
	{
		VolumeModulation_DEPRECATED.Distribution = NULL;
		PitchModulation_DEPRECATED.Distribution = NULL;
	}
#endif
}

void USoundNodeModulator::ParseNodes( UAudioDevice* AudioDevice, USoundNode* Parent, INT ChildIndex, UAudioComponent* AudioComponent, TArray<FWaveInstance*>& WaveInstances )
{
	RETRIEVE_SOUNDNODE_PAYLOAD( sizeof( FLOAT ) + sizeof( FLOAT ) );
	DECLARE_SOUNDNODE_ELEMENT( FLOAT, UsedVolumeModulation );
	DECLARE_SOUNDNODE_ELEMENT( FLOAT, UsedPitchModulation );

	if( *RequiresInitialization )
	{
		UsedVolumeModulation = VolumeMax + ( ( VolumeMin - VolumeMax ) * appSRand() );
		UsedPitchModulation = PitchMax + ( ( PitchMin - PitchMax ) * appSRand() );

		*RequiresInitialization = 0;
	}
	
	AudioComponent->CurrentVolume *= UsedVolumeModulation;
	AudioComponent->CurrentPitch *= UsedPitchModulation;

	Super::ParseNodes( AudioDevice, Parent, ChildIndex, AudioComponent, WaveInstances );
}

/** 
 * Used to create a unique string to identify unique nodes
 */
FString USoundNodeModulator::GetUniqueString( void )
{
	FString Unique = TEXT( "Modulator" );

	Unique += FString::Printf( TEXT( " %g %g %g %g" ), VolumeMin, VolumeMax, PitchMin, PitchMax );

	Unique += TEXT( "/" );
	return( Unique );
}

IMPLEMENT_CLASS( USoundNodeModulator );

/*-----------------------------------------------------------------------------
	USoundNodeModulatorContinuous implementation.
-----------------------------------------------------------------------------*/

void USoundNodeModulatorContinuous::Serialize( FArchive& Ar )
{
	Super::Serialize( Ar );
}

void USoundNodeModulatorContinuous::ParseNodes( UAudioDevice* AudioDevice, USoundNode* Parent, INT ChildIndex, UAudioComponent* AudioComponent, TArray<FWaveInstance*>& WaveInstances )
{
	RETRIEVE_SOUNDNODE_PAYLOAD( sizeof( FLOAT ) + sizeof( FLOAT ) );
	DECLARE_SOUNDNODE_ELEMENT( FLOAT, UsedVolumeModulation );
	DECLARE_SOUNDNODE_ELEMENT( FLOAT, UsedPitchModulation );

	UsedVolumeModulation = VolumeModulation.GetValue( AudioComponent->PlaybackTime, AudioComponent );
	UsedPitchModulation = PitchModulation.GetValue( AudioComponent->PlaybackTime, AudioComponent );

	AudioComponent->CurrentVolume *= UsedVolumeModulation;
	AudioComponent->CurrentPitch *= UsedPitchModulation;

	Super::ParseNodes( AudioDevice, Parent, ChildIndex, AudioComponent, WaveInstances );
}

/** 
 * Used to create a unique string to identify unique nodes
 */
FString USoundNodeModulatorContinuous::GetUniqueString( void )
{
	FString Unique = TEXT( "ModulatorContinuous" );

	Unique += TEXT( "Complex" );

	Unique += TEXT( "/" );
	return( Unique );
}

IMPLEMENT_CLASS( USoundNodeModulatorContinuous );

/*-----------------------------------------------------------------------------
	USoundNodeAmbient implementation.
-----------------------------------------------------------------------------*/

void USoundNodeAmbient::ParseNodes( UAudioDevice* AudioDevice, USoundNode* Parent, INT ChildIndex, UAudioComponent* AudioComponent, TArray<FWaveInstance*>& WaveInstances )
{
	RETRIEVE_SOUNDNODE_PAYLOAD( sizeof( FLOAT ) + sizeof( FLOAT ) );
	DECLARE_SOUNDNODE_ELEMENT( FLOAT, UsedVolumeModulation );
	DECLARE_SOUNDNODE_ELEMENT( FLOAT, UsedPitchModulation );

	if( *RequiresInitialization )
	{
		UsedVolumeModulation = VolumeMax + ( ( VolumeMin - VolumeMax ) * appSRand() );
		UsedPitchModulation = PitchMax + ( ( PitchMin - PitchMax ) * appSRand() );
		*RequiresInitialization = 0;
	}

	const FLOAT Distance = FDist( AudioComponent->CurrentLocation, AudioComponent->Listener->Location );

	if( bAttenuate )
	{
		CalculateAttenuatedVolume( AudioComponent, DistanceModel, Distance, RadiusMin, RadiusMax, dBAttenuationAtMax );
	}

	// Default to no low pass filter
	if( bAttenuateWithLPF )
	{
		CalculateLPFComponent( AudioComponent, Distance, LPFRadiusMin, LPFRadiusMax );
	}

	AudioComponent->CurrentUseSpatialization |= bSpatialize;
	AudioComponent->CurrentVolume *= UsedVolumeModulation;
	AudioComponent->CurrentPitch *= UsedPitchModulation;

	// Make sure we're getting notified when the sounds stops.
	AudioComponent->CurrentNotifyBufferFinishedHook = this;

	for( INT SlotIndex = 0; SlotIndex < SoundSlots.Num(); SlotIndex++ )
	{
		// If slot has a wave, start playing it
		if( SoundSlots( SlotIndex ).Wave != NULL )
		{
			FLOAT OriginalVolume = AudioComponent->CurrentVolume;
			FLOAT OriginalPitch = AudioComponent->CurrentPitch;

			AudioComponent->CurrentVolume *= SoundSlots( SlotIndex ).VolumeScale;
			AudioComponent->CurrentPitch *= SoundSlots( SlotIndex ).PitchScale;

			const INT PreviousNum = WaveInstances.Num();
			SoundSlots( SlotIndex ).Wave->ParseNodes( AudioDevice, this, SlotIndex, AudioComponent, WaveInstances );

			// Mark wave instance associated with this "Wave" to loop indefinitely.
			for( INT i = PreviousNum; i < WaveInstances.Num(); i++ )
			{
				FWaveInstance* WaveInstance = WaveInstances( i );
				WaveInstance->LoopingMode = LOOP_Forever;
			}

			AudioComponent->CurrentVolume = OriginalVolume;
			AudioComponent->CurrentPitch = OriginalPitch;
		}
	}
}

void USoundNodeAmbient::GetNodes( UAudioComponent* AudioComponent, TArray<USoundNode*>& SoundNodes )
{
	SoundNodes.AddItem( this );

	for( INT SlotIndex = 0; SlotIndex < SoundSlots.Num(); SlotIndex++ )
	{
		if( SoundSlots( SlotIndex ).Wave )
		{
			SoundSlots( SlotIndex ).Wave->GetNodes( AudioComponent, SoundNodes );
		}
	}
}

void USoundNodeAmbient::GetAllNodes( TArray<USoundNode*>& SoundNodes )
{
	SoundNodes.AddItem( this );

	for( INT SlotIndex = 0; SlotIndex < SoundSlots.Num(); SlotIndex++ )
	{
		if( SoundSlots( SlotIndex ).Wave )
		{
			SoundSlots( SlotIndex ).Wave->GetAllNodes( SoundNodes );
		}
	}
}

/**
 * Notifies the sound node that a wave instance in its subtree has finished.
 *
 * @param WaveInstance	WaveInstance that was finished 
 */
UBOOL USoundNodeAmbient::NotifyWaveInstanceFinished( FWaveInstance* WaveInstance ) 
{
	// Mark wave instance associated with wave as not yet finished.
	WaveInstance->bIsStarted = TRUE;
	WaveInstance->bIsFinished = FALSE;

	return( FALSE );
}

/** 
 * Used to create a unique string to identify unique nodes
 */
FString USoundNodeAmbient::GetUniqueString( void )
{
	FString Unique = TEXT( "Ambient" );

	Unique += TEXT( "Complex" );

	Unique += TEXT( "/" );
	return( Unique );
}

IMPLEMENT_CLASS( USoundNodeAmbient );

/*-----------------------------------------------------------------------------
	USoundNodeAmbientNonLoop implementation.
-----------------------------------------------------------------------------*/

void USoundNodeAmbientNonLoop::Serialize( FArchive& Ar )
{
	Super::Serialize( Ar );
	if( Ar.Ver() < VER_DEPRECATE_SOUND_DISTRIBUTIONS )
	{
		DelayTime_DEPRECATED.GetOutRange( DelayMin, DelayMax );
	}

#if WITH_EDITOR
	if( ( GIsCooking == TRUE ) && Ar.IsLoading() )
	{
		DelayTime_DEPRECATED.Distribution = NULL;
	}
#endif
}

void USoundNodeAmbientNonLoop::ParseNodes( UAudioDevice* AudioDevice, USoundNode* Parent, INT ChildIndex, UAudioComponent* AudioComponent, TArray<FWaveInstance*>& WaveInstances )
{
	RETRIEVE_SOUNDNODE_PAYLOAD( sizeof( FLOAT ) + sizeof( FLOAT ) + sizeof( FLOAT ) + sizeof( INT ) );
	DECLARE_SOUNDNODE_ELEMENT( FLOAT, UsedVolumeModulation );
	DECLARE_SOUNDNODE_ELEMENT( FLOAT, UsedPitchModulation );
	DECLARE_SOUNDNODE_ELEMENT( FLOAT, NextSoundTime );
	DECLARE_SOUNDNODE_ELEMENT( INT, SlotIndex );

	if( *RequiresInitialization )
	{
		UsedVolumeModulation = VolumeMax + ( ( VolumeMin - VolumeMax ) * appSRand() );
		UsedPitchModulation = PitchMax + ( ( PitchMin - PitchMax ) * appSRand() );
		NextSoundTime = AudioComponent->PlaybackTime + DelayMax + ( ( DelayMin - DelayMax ) * appSRand() );

		SlotIndex = PickNextSlot();

		*RequiresInitialization = 0;
	}

	const FLOAT Distance = FDist( AudioComponent->CurrentLocation, AudioComponent->Listener->Location );

	if( bAttenuate )
	{
		CalculateAttenuatedVolume( AudioComponent, DistanceModel, Distance, RadiusMin, RadiusMax, dBAttenuationAtMax );
	}

	// Default to no low pass filter
	if( bAttenuateWithLPF )
	{
		CalculateLPFComponent( AudioComponent, Distance, LPFRadiusMin, LPFRadiusMax );
	}

	AudioComponent->CurrentUseSpatialization |= bSpatialize;
	AudioComponent->CurrentVolume *= UsedVolumeModulation;
	AudioComponent->CurrentPitch *= UsedPitchModulation;

	// Apply per-slot volume and pitch modifiers.
	if( SlotIndex < SoundSlots.Num() )
	{
		AudioComponent->CurrentVolume *= SoundSlots( SlotIndex ).VolumeScale;
		AudioComponent->CurrentPitch *= SoundSlots( SlotIndex ).PitchScale;
	}

	// Make sure we're getting notified when the sounds stops.
	AudioComponent->CurrentNotifyBufferFinishedHook = this;

	// Never finished.
	AudioComponent->bFinished = FALSE;

	// If we should currently be playing a sound, parse the current slot
	if( AudioComponent->PlaybackTime >= NextSoundTime )
	{
		// If slot index is in range and and a valid wave.
		if( SlotIndex < SoundSlots.Num() )
		{
			// If slot has a wave, start playing it
			if( SoundSlots( SlotIndex ).Wave != NULL )
			{
				SoundSlots( SlotIndex ).Wave->ParseNodes( AudioDevice, this, SlotIndex, AudioComponent, WaveInstances );
			}
			// If it doesn't - move the NextSoundTime on again and pick a different slot right now.
			else
			{
				NextSoundTime = AudioComponent->PlaybackTime + DelayMax + ( ( DelayMin - DelayMax ) * appSRand() );
				SlotIndex = PickNextSlot();
			}
		}
	}
}

/**
 * Notifies the sound node that a wave instance in its subtree has finished.
 *
 * @param WaveInstance	WaveInstance that was finished 
 */
UBOOL USoundNodeAmbientNonLoop::NotifyWaveInstanceFinished( FWaveInstance* WaveInstance ) 
{
	UAudioComponent* AudioComponent = WaveInstance->AudioComponent;
	RETRIEVE_SOUNDNODE_PAYLOAD( sizeof( FLOAT ) + sizeof( FLOAT ) + sizeof( FLOAT ) + sizeof( INT ) );
	DECLARE_SOUNDNODE_ELEMENT( FLOAT, UsedVolumeModulation );
	DECLARE_SOUNDNODE_ELEMENT( FLOAT, UsedPitchModulation );
	DECLARE_SOUNDNODE_ELEMENT( FLOAT, NextSoundTime );
	DECLARE_SOUNDNODE_ELEMENT( INT, SlotIndex );

	// Choose the various parameters again.
	UsedVolumeModulation = VolumeMax + ( ( VolumeMin - VolumeMax ) * appSRand() );
	UsedPitchModulation = PitchMax + ( ( PitchMin - PitchMax ) * appSRand() );

	// Pick next time to play a sound
	NextSoundTime = AudioComponent->PlaybackTime + DelayMax + ( ( DelayMin - DelayMax ) * appSRand() );

	// Allow wave instance to be played again the next iteration.
	WaveInstance->bIsStarted = TRUE;
	WaveInstance->bIsFinished = FALSE;

	// Pick next slot to play
	SlotIndex = PickNextSlot();

	return( FALSE );
}

INT USoundNodeAmbientNonLoop::PickNextSlot( void )
{
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
	FLOAT ChosenWeight = appFrand() * TotalWeight;

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
 * Used to create a unique string to identify unique nodes
 */
FString USoundNodeAmbientNonLoop::GetUniqueString( void )
{
	FString Unique = TEXT( "AmbientNonLoop" );

	Unique += TEXT( "Complex" );

	Unique += TEXT( "/" );
	return( Unique );
}

IMPLEMENT_CLASS( USoundNodeAmbientNonLoop );

/*-----------------------------------------------------------------------------
	USoundNodeAmbientNonLoopToggle implementation
-----------------------------------------------------------------------------*/

/**
 * Notifies the sound node that a wave instance in its subtree has finished.
 *
 * @param WaveInstance	WaveInstance that was finished 
 */
UBOOL USoundNodeAmbientNonLoopToggle::NotifyWaveInstanceFinished( FWaveInstance* WaveInstance ) 
{
	UBOOL retVal = Super::NotifyWaveInstanceFinished(WaveInstance);
	// Do not loop the audio
	if(WaveInstance->AudioComponent->IsPlaying())
	{
		// If toggled off, AudioComponent will already be stopped, so only do this if reached the end of wave instance
		WaveInstance->AudioComponent->Stop();
	}
	return retVal;
}

/** 
 * Used to create a unique string to identify unique nodes
 */
FString USoundNodeAmbientNonLoopToggle::GetUniqueString( void )
{
	FString Unique = TEXT( "AmbientNonLoopToggle" );

	Unique += TEXT( "Complex" );

	Unique += TEXT( "/" );
	return( Unique );
}

IMPLEMENT_CLASS( USoundNodeAmbientNonLoopToggle );

/*-----------------------------------------------------------------------------
	USoundNodeWaveParam implementation
-----------------------------------------------------------------------------*/

/** 
 * Gets the time in seconds of the associated sound data
 */	
FLOAT USoundNodeWaveParam::GetDuration( void )
{
	debugfSuppressed( NAME_DevAudioVerbose, TEXT( "Set the duration of the owning sound cue. There is no way to calculate it from here." ) );
	return( 0.0f );
}

/** 
 * USoundNode interface.
 */
void USoundNodeWaveParam::ParseNodes( UAudioDevice* AudioDevice, USoundNode* Parent, INT ChildIndex, UAudioComponent* AudioComponent, TArray<FWaveInstance*>& WaveInstances )
{
	USoundNodeWave* NewWave = NULL;
	AudioComponent->GetWaveParameter( WaveParameterName, NewWave );
	if( NewWave != NULL )
	{
		NewWave->ParseNodes( AudioDevice, this, -1, AudioComponent, WaveInstances );
	}
	else
	{
		// use the default node linked to us, if any
		Super::ParseNodes( AudioDevice, Parent, ChildIndex, AudioComponent, WaveInstances );
	}
}

/** 
 *
 */
void USoundNodeWaveParam::GetNodes( UAudioComponent* AudioComponent, TArray<USoundNode*>& SoundNodes )
{
	SoundNodes.AddItem( this );
	USoundNodeWave* NewWave = NULL;
	AudioComponent->GetWaveParameter( WaveParameterName, NewWave );
	if( NewWave != NULL )
	{
		NewWave->GetNodes( AudioComponent, SoundNodes );
	}
	else
	{
		// use the default node linked to us, if any
		Super::GetNodes( AudioComponent, SoundNodes );
	}
}

/** 
 * Used to create a unique string to identify unique nodes
 */
FString USoundNodeWaveParam::GetUniqueString( void )
{
	FString Unique = TEXT( "WaveParam" );

	Unique += TEXT( "/" );
	return( Unique );
}

IMPLEMENT_CLASS( USoundNodeWaveParam );

/*-----------------------------------------------------------------------------
    USoundNodeRandom implementation.
-----------------------------------------------------------------------------*/

void USoundNodeRandom::FixWeightsArray()
{
	// If weights and children got out of sync, we fix it first.
	if( Weights.Num() < ChildNodes.Num() )
	{
		Weights.AddZeroed( ChildNodes.Num() - Weights.Num() );
	}
	else if( Weights.Num() > ChildNodes.Num() )
	{
		const INT NumToRemove = Weights.Num() - ChildNodes.Num();
		Weights.Remove( Weights.Num() - NumToRemove, NumToRemove );
	}
}

void USoundNodeRandom::FixHasBeenUsedArray()
{
	// If HasBeenUsed and children got out of sync, we fix it first.
	if( HasBeenUsed.Num() < ChildNodes.Num() )
	{
		HasBeenUsed.AddZeroed( ChildNodes.Num() - HasBeenUsed.Num() );
	}
	else if( HasBeenUsed.Num() > ChildNodes.Num() )
	{
		const INT NumToRemove = HasBeenUsed.Num() - ChildNodes.Num();
		HasBeenUsed.Remove( HasBeenUsed.Num() - NumToRemove, NumToRemove );
	}
}

/**
 * Called after object and all its dependencies have been serialized.
 */
void USoundNodeRandom::PostLoad()
{
	Super::PostLoad();
	if (!GIsEditor && PreselectAtLevelLoad > 0)
	{
		while (ChildNodes.Num() > PreselectAtLevelLoad)
		{
			RemoveChildNode(appRand() % ChildNodes.Num());
		}
	}
}

void USoundNodeRandom::ParseNodes( UAudioDevice* AudioDevice, USoundNode* Parent, INT ChildIndex, UAudioComponent* AudioComponent, TArray<FWaveInstance*>& WaveInstances )
{
	RETRIEVE_SOUNDNODE_PAYLOAD( sizeof( INT ) );
	DECLARE_SOUNDNODE_ELEMENT( INT, NodeIndex );

	// A random sound node cannot use seamless looping as it might switch to another wave the next iteration.
	AudioComponent->CurrentNotifyOnLoop = FALSE;

	if( bRandomizeWithoutReplacement == TRUE )
	{
		FixHasBeenUsedArray();  // for now prob need this until resave packages has occurred
	}

	// Pick a random child node and save the index.
	if( *RequiresInitialization )
	{
		NodeIndex = 0;	
		FLOAT WeightSum = 0.0f;

		// only calculate the weights that have not been used and use that set for the random choice
		for( INT i = 0; i < Weights.Num(); i++ )
		{
			if( ( bRandomizeWithoutReplacement == TRUE ) && ( HasBeenUsed( i ) != TRUE ) )
			{
				WeightSum += Weights( i );
			}
			else if( bRandomizeWithoutReplacement == FALSE ) 
			{
				WeightSum += Weights( i );
			}
		}

		FLOAT Weight = appFrand() * WeightSum;
		for( INT i = 0; i < ChildNodes.Num() && i < Weights.Num(); i++ )
		{
			if( bRandomizeWithoutReplacement && ( Weights( i ) >= Weight ) && ( HasBeenUsed( i ) != TRUE ) )
			{
				HasBeenUsed( i ) = TRUE;
				// we played a sound so increment how many sounds we have played	
				NumRandomUsed++; 

				NodeIndex = i;
				break;

			}
			else if( ( bRandomizeWithoutReplacement == FALSE ) && ( Weights( i ) >= Weight ) )
			{
				NodeIndex = i;
				break;
			}
			else
			{
				Weight -= Weights( i );
			}
		}

		*RequiresInitialization = 0;
	}

	// check to see if we have used up our random sounds
	if( bRandomizeWithoutReplacement && ( HasBeenUsed.Num() > 0 ) && ( NumRandomUsed >= HasBeenUsed.Num() )	)
	{
		// reset all of the children nodes
		for( INT i = 0; i < HasBeenUsed.Num(); ++i )
		{
			HasBeenUsed( i ) = FALSE;
		}

		// set the node that has JUST played to be TRUE so we don't repeat it
		HasBeenUsed( NodeIndex ) = TRUE;
		NumRandomUsed = 1;
	}

	// "play" the sound node that was selected
	if( NodeIndex < ChildNodes.Num() && ChildNodes( NodeIndex ) )
	{
		ChildNodes( NodeIndex )->ParseNodes( AudioDevice, this, NodeIndex, AudioComponent, WaveInstances );	
	}
}

void USoundNodeRandom::GetNodes( UAudioComponent* AudioComponent, TArray<USoundNode*>& SoundNodes )
{
	RETRIEVE_SOUNDNODE_PAYLOAD( sizeof( INT ) );
	DECLARE_SOUNDNODE_ELEMENT( INT, NodeIndex );

	if( !*RequiresInitialization )
	{
		SoundNodes.AddItem( this );
		if( NodeIndex < ChildNodes.Num() && ChildNodes( NodeIndex ) )
		{
			ChildNodes( NodeIndex )->GetNodes( AudioComponent, SoundNodes );	
		}
	}
}

/**
 * Called by the Sound Cue Editor for nodes which allow children.  The default behavior is to
 * attach a single connector. Derived classes can override to e.g. add multiple connectors.
 **/
void USoundNodeRandom::CreateStartingConnectors()
{
	// Random Sound Nodes default with two connectors.
	InsertChildNode( ChildNodes.Num() );
	InsertChildNode( ChildNodes.Num() );
}

void USoundNodeRandom::InsertChildNode( INT Index )
{
	FixWeightsArray();
	FixHasBeenUsedArray();

	check( Index >= 0 && Index <= Weights.Num() );
	check( ChildNodes.Num() == Weights.Num() );

	Weights.Insert( Index );
	Weights(Index) = 1.0f;

	HasBeenUsed.Insert( Index );
	HasBeenUsed( Index ) = FALSE;

	Super::InsertChildNode( Index );
}

void USoundNodeRandom::RemoveChildNode( INT Index )
{
	FixWeightsArray();
	FixHasBeenUsedArray();

	check( Index >= 0 && Index < Weights.Num() );
	check( ChildNodes.Num() == Weights.Num() );

	Weights.Remove( Index );
	HasBeenUsed.Remove( Index );

	Super::RemoveChildNode( Index );
}

/** 
 * Used to create a unique string to identify unique nodes
 */
FString USoundNodeRandom::GetUniqueString( void )
{
	FString Unique = TEXT( "Random" );

	Unique += bRandomizeWithoutReplacement ? TEXT( " True" ) : TEXT( " False" );

	Unique += TEXT( "/" );
	return( Unique );
}

IMPLEMENT_CLASS( USoundNodeRandom );

/*-----------------------------------------------------------------------------
         USoundNodeDelay implementation.
-----------------------------------------------------------------------------*/

void USoundNodeDelay::Serialize( FArchive& Ar )
{
	Super::Serialize( Ar );
	if( Ar.Ver() < VER_DEPRECATE_SOUND_DISTRIBUTIONS )
	{
		DelayDuration_DEPRECATED.GetOutRange( DelayMin, DelayMax );
	}

#if WITH_EDITOR
	if( ( GIsCooking == TRUE ) && Ar.IsLoading() )
	{
		DelayDuration_DEPRECATED.Distribution = NULL;
	}
#endif
}

/**
 * The SoundNodeDelay will delay randomly from min to max seconds.
 *
 * Once the delay has passed it will then tell all of its children nods
 * to go ahead and be "parsed" / "play"
 *
 **/
void USoundNodeDelay::ParseNodes( UAudioDevice* AudioDevice, USoundNode* Parent, INT ChildIndex, UAudioComponent* AudioComponent, TArray<FWaveInstance*>& WaveInstances )
{
	RETRIEVE_SOUNDNODE_PAYLOAD( sizeof( FLOAT ) + sizeof( FLOAT ) );
	DECLARE_SOUNDNODE_ELEMENT( FLOAT, ActualDelay );
	DECLARE_SOUNDNODE_ELEMENT( FLOAT, StartOfDelay );

	// A delay node cannot use seamless looping as it introduces a delay.
	AudioComponent->CurrentNotifyOnLoop = FALSE;

	// Check to see if this is the first time through.
	if( *RequiresInitialization )
	{
		ActualDelay	= DelayMax + ( ( DelayMin - DelayMax ) * appSRand() );
		StartOfDelay = AudioComponent->PlaybackTime;

		*RequiresInitialization = FALSE;
	}

	FLOAT TimeSpentWaiting = AudioComponent->PlaybackTime - StartOfDelay;

	// If we have not waited long enough then just keep waiting.
	if( TimeSpentWaiting < ActualDelay )
	{
		// We're not finished even though we might not have any wave instances in flight.
		AudioComponent->bFinished = FALSE;
	}
	// Go ahead and play the sound.
	else
	{
		Super::ParseNodes( AudioDevice, Parent, ChildIndex, AudioComponent, WaveInstances );
	}
}

/**
 * Returns the maximum duration this sound node will play for. 
 * 
 * @return maximum duration this sound will play for
 */
FLOAT USoundNodeDelay::GetDuration( void )
{
	// Get length of child node.
	FLOAT ChildDuration = 0.0f;
	if( ChildNodes( 0 ) )
	{
		ChildDuration = ChildNodes( 0 )->GetDuration();
	}

	// And return the two together.
	return( ChildDuration + DelayMax );
}

/** 
 * Used to create a unique string to identify unique nodes
 */
FString USoundNodeDelay::GetUniqueString( void )
{
	FString Unique = TEXT( "Delay" );

	Unique += FString::Printf( TEXT( " %g %g" ), DelayMin, DelayMax );

	Unique += TEXT( "/" );
	return( Unique );
}

IMPLEMENT_CLASS( USoundNodeDelay );

/*-----------------------------------------------------------------------------
         USoundNodeConcatenator implementation.
-----------------------------------------------------------------------------*/

/**
 * Returns whether the node is finished after having been notified of buffer
 * being finished.
 *
 * @param	AudioComponent	Audio component containing payload data
 * @return	TRUE if finished, FALSE otherwise.
 */
UBOOL USoundNodeConcatenator::IsFinished( UAudioComponent* AudioComponent ) 
{ 
	RETRIEVE_SOUNDNODE_PAYLOAD( sizeof( INT ) );
	DECLARE_SOUNDNODE_ELEMENT( INT, NodeIndex );
	check( *RequiresInitialization == 0 );

	return( NodeIndex >= ChildNodes.Num() ? TRUE : FALSE ); 
}

/**
 * Notifies the sound node that a wave instance in its subtree has finished.
 *
 * @param WaveInstance	WaveInstance that was finished 
 */
UBOOL USoundNodeConcatenator::NotifyWaveInstanceFinished( FWaveInstance* WaveInstance )
{
	UAudioComponent* AudioComponent = WaveInstance->AudioComponent;
	RETRIEVE_SOUNDNODE_PAYLOAD( sizeof( INT ) );
	DECLARE_SOUNDNODE_ELEMENT( INT, NodeIndex );
	check( *RequiresInitialization == 0 );
	
	// Allow wave instance to be played again the next iteration.
	WaveInstance->bIsStarted = TRUE;
	WaveInstance->bIsFinished = FALSE;

	// Advance index.
	NodeIndex++;

	return( FALSE );
}

/**
 * Returns the maximum duration this sound node will play for. 
 * 
 * @return maximum duration this sound will play for
 */
FLOAT USoundNodeConcatenator::GetDuration( void )
{
	// Sum up length of child nodes.
	FLOAT Duration = 0.0f;
	for( INT ChildNodeIndex = 0; ChildNodeIndex < ChildNodes.Num(); ChildNodeIndex++ )
	{
		USoundNode* ChildNode = ChildNodes( ChildNodeIndex );
		if( ChildNode )
		{
			Duration += ChildNode->GetDuration();
		}
	}
	return Duration;
}

/**
 * Concatenators have two connectors by default.
 */
void USoundNodeConcatenator::CreateStartingConnectors( void )
{
	// Concatenators default to two two connectors.
	InsertChildNode( ChildNodes.Num() );
	InsertChildNode( ChildNodes.Num() );
}

/**
 * Overloaded to add an entry to InputVolume.
 */
void USoundNodeConcatenator::InsertChildNode( INT Index )
{
	Super::InsertChildNode( Index );
	InputVolume.Insert( Index );
	InputVolume( Index ) = 1.0f;
}

/**
 * Overloaded to remove an entry from InputVolume.
 */
void USoundNodeConcatenator::RemoveChildNode( INT Index )
{
	Super::RemoveChildNode( Index );
	InputVolume.Remove( Index );
}

void USoundNodeConcatenator::ParseNodes( UAudioDevice* AudioDevice, USoundNode* Parent, INT ChildIndex, UAudioComponent* AudioComponent, TArray<FWaveInstance*>& WaveInstances )
{
	RETRIEVE_SOUNDNODE_PAYLOAD( sizeof( INT ) );
	DECLARE_SOUNDNODE_ELEMENT( INT, NodeIndex );

	// Start from the beginning.
	if( *RequiresInitialization )
	{
		NodeIndex = 0;	
		*RequiresInitialization = FALSE;
	}

	// Play the current node.
	if( NodeIndex < ChildNodes.Num() )
	{
		// A concatenator cannot use seamless looping as it will switch to another wave once the current one is finished.
		AudioComponent->CurrentNotifyOnLoop = FALSE;

		// Don't set notification hook for the last entry so hooks higher up the chain get called.
		if( NodeIndex < ChildNodes.Num() - 1 )
		{
			AudioComponent->CurrentNotifyBufferFinishedHook = this;
		}

		// Play currently active node.
		USoundNode* ChildNode = ChildNodes( NodeIndex );
		if( ChildNode )
		{
			FAudioComponentSavedState SavedState;
			SavedState.Set( AudioComponent );

			AudioComponent->CurrentVolume *= InputVolume( NodeIndex );
			ChildNode->ParseNodes( AudioDevice, this, NodeIndex, AudioComponent, WaveInstances );

			SavedState.Restore( AudioComponent );
		}
	}
}

void USoundNodeConcatenator::GetNodes( UAudioComponent* AudioComponent, TArray<USoundNode*>& SoundNodes )
{
	RETRIEVE_SOUNDNODE_PAYLOAD( sizeof( INT ) );
	DECLARE_SOUNDNODE_ELEMENT( INT, NodeIndex );

	if( !*RequiresInitialization )
	{
		SoundNodes.AddItem( this );
		if( NodeIndex < ChildNodes.Num() )
		{
			USoundNode* ChildNode = ChildNodes(NodeIndex);
			if( ChildNode )
			{
				ChildNode->GetNodes( AudioComponent, SoundNodes );	
			}
		}
	}
}

/** 
 * Used to create a unique string to identify unique nodes
 */
FString USoundNodeConcatenator::GetUniqueString( void )
{
	FString Unique = TEXT( "Concatenator" );

	for( INT Index = 0; Index < InputVolume.Num(); Index++ )
	{
		Unique += FString::Printf( TEXT( " %g" ), InputVolume( Index ) );
	}

	Unique += TEXT( "/" );
	return( Unique );
}

IMPLEMENT_CLASS( USoundNodeConcatenator );

/*-----------------------------------------------------------------------------
         USoundNodeConcatenatorRadio implementation.
-----------------------------------------------------------------------------*/

/**
 * Returns whether the node is finished after having been notified of buffer
 * being finished.
 *
 * @param	AudioComponent	Audio component containing payload data
 * @return	TRUE if finished, FALSE otherwise.
 */
UBOOL USoundNodeConcatenatorRadio::IsFinished( UAudioComponent* AudioComponent ) 
{ 
	RETRIEVE_SOUNDNODE_PAYLOAD( sizeof( INT ) );
	DECLARE_SOUNDNODE_ELEMENT( INT, NodeIndex );
	check( *RequiresInitialization == 0 );

	return( NodeIndex > 2 ); 
}

/**
 * Notifies the sound node that a wave instance in its subtree has finished.
 *
 * @param WaveInstance	WaveInstance that was finished 
 */
UBOOL USoundNodeConcatenatorRadio::NotifyWaveInstanceFinished( FWaveInstance* WaveInstance )
{
	UAudioComponent* AudioComponent = WaveInstance->AudioComponent;
	RETRIEVE_SOUNDNODE_PAYLOAD( sizeof( INT ) );
	DECLARE_SOUNDNODE_ELEMENT( INT, NodeIndex );
	check( *RequiresInitialization == 0 );
	
	// Allow wave instance to be played again the next iteration.
	WaveInstance->bIsStarted = TRUE;
	WaveInstance->bIsFinished = FALSE;

	// Advance index.
	NodeIndex++;

	return( FALSE );
}

/**
 * Returns the maximum duration this sound node will play for. 
 * 
 * @return maximum duration this sound will play for
 */
FLOAT USoundNodeConcatenatorRadio::GetDuration( void )
{
	// Sum up length of child nodes.
	FLOAT Duration = 0.0f;
	USoundNode* ChildNode = ChildNodes( 0 );
	if( ChildNode )
	{
		Duration += ChildNode->GetDuration();
	}

	UAudioDevice* AudioDevice = GEngine->Client->GetAudioDevice();
	if( AudioDevice->ChirpInSoundNodeWave )
	{
		Duration += AudioDevice->ChirpInSoundNodeWave->GetDuration();
	}

	if( AudioDevice->ChirpOutSoundNodeWave )
	{
		Duration += AudioDevice->ChirpOutSoundNodeWave->GetDuration();
	}

	return Duration;
}

/**
 * Concatenators for radio have only 1 child
 */
void USoundNodeConcatenatorRadio::CreateStartingConnectors( void )
{
	// Radios default to chirp in, dialog, chirp out - the chirps are predefined
	InsertChildNode( ChildNodes.Num() );
}

/** 
 * Returns TRUE if the radio chirp sound should be played
 */
UBOOL USoundNodeConcatenatorRadio::ApplyChirpSound( UAudioDevice* AudioDevice, UAudioComponent* AudioComponent, USoundNodeWave* Chirp )
{
	if( !Chirp )
	{
		debugf( NAME_DevAudio, TEXT( " ... no chirp sound" ) );
		return( FALSE );
	}

	return( ApplyRadioFilter( AudioDevice, AudioComponent ) );
}

void USoundNodeConcatenatorRadio::ParseNodes( UAudioDevice* AudioDevice, USoundNode* Parent, INT ChildIndex, UAudioComponent* AudioComponent, TArray<FWaveInstance*>& WaveInstances )
{
	RETRIEVE_SOUNDNODE_PAYLOAD( sizeof( INT ) );
	DECLARE_SOUNDNODE_ELEMENT( INT, NodeIndex );

	// Start from the beginning.
	if( *RequiresInitialization )
	{
		NodeIndex = 0;	
		*RequiresInitialization = FALSE;
	}

	USoundNode* ChildNode = NULL;
	switch( NodeIndex )
	{
	case 0:
		if( ApplyChirpSound( AudioDevice, AudioComponent, AudioDevice->ChirpInSoundNodeWave ) )
		{
			ChildNode = AudioDevice->ChirpInSoundNodeWave;
		}
		else
		{
			// If no chirp in sound, or radio not applied, play the dialog line straight away and disable the radio filter so the chirp out does not play
			NodeIndex = 1;
			ChildNode = ChildNodes( 0 );
		}

		AudioComponent->bRadioFilterSelected = TRUE;
		break;

	case 1:
		ChildNode = ChildNodes( 0 );
		break;

	case 2:
		if( AudioComponent->bApplyRadioFilter )
		{
			ChildNode = AudioDevice->ChirpOutSoundNodeWave;
		}
		else
		{
			// Mark the cue as finished
			NodeIndex = 3;
		}
		break;
	}

	// Play the current node.
	if( ChildNode )
	{
		AudioComponent->CurrentNotifyBufferFinishedHook = this;

		FAudioComponentSavedState SavedState;
		SavedState.Set( AudioComponent );

		ChildNode->ParseNodes( AudioDevice, this, NodeIndex, AudioComponent, WaveInstances );

		SavedState.Restore( AudioComponent );
	}
}

void USoundNodeConcatenatorRadio::GetNodes( UAudioComponent* AudioComponent, TArray<USoundNode*>& SoundNodes )
{
	RETRIEVE_SOUNDNODE_PAYLOAD( sizeof( INT ) );
	DECLARE_SOUNDNODE_ELEMENT( INT, NodeIndex );

	if( !*RequiresInitialization )
	{
		SoundNodes.AddItem( this );
		USoundNode* ChildNode = ChildNodes( 0 );
		if( ChildNode )
		{
			ChildNode->GetNodes( AudioComponent, SoundNodes );	
		}
	}
}

/** 
 * Used to create a unique string to identify unique nodes
 */
FString USoundNodeConcatenatorRadio::GetUniqueString( void )
{
	FString Unique = TEXT( "ConcatenatorRadio" );

	Unique += TEXT( "/" );
	return( Unique );
}

IMPLEMENT_CLASS( USoundNodeConcatenatorRadio );

/*-----------------------------------------------------------------------------
       USoundNodeMature implementation.
-----------------------------------------------------------------------------*/

INT USoundNodeMature::GetMaxChildNodes( void )
{
	// This node can have any number of children.
	return( -1 );
}

void USoundNodeMature::GetNodes( UAudioComponent* AudioComponent, TArray<USoundNode*>& SoundNodes )
{
	RETRIEVE_SOUNDNODE_PAYLOAD( sizeof( INT ) );
	DECLARE_SOUNDNODE_ELEMENT( INT, NodeIndex );

	if( !*RequiresInitialization )
	{
		SoundNodes.AddItem( this );
		if( NodeIndex < ChildNodes.Num() && ChildNodes( NodeIndex ) )
		{
			ChildNodes(NodeIndex)->GetNodes( AudioComponent, SoundNodes );	
		}
	}
}

enum EMaturityChildType
{
	ChildType_None		= 0,
	ChildType_Mature	= 1,
	ChildType_NonMature	= 2,
};

/**
 * Recursively traverses the sound nodes until it finds a sound wave to evaluate its maturity level.
 *
 * @param	Node	The sound node to start iterating from.
 * 
 * @return	ChildType_Mature if the first sound wave is a mature sound; 
 *			ChildType_NonMature if it is not mature; 
 * 			ChildType_None if no sound wave was found.
 */
EMaturityChildType GetMaturityTypeForChild( USoundNode* Node )
{
	EMaturityChildType Type = ChildType_None;

	if( Node )
	{
		// Try to see if the given node is a sound wave node. 
		if( Node->IsA( USoundNodeWave::StaticClass() ) )
		{
			USoundNodeWave* Wave = CastChecked<USoundNodeWave>( Node );
			if( Wave->bMature )
			{
				Type = ChildType_Mature;
			}
			else
			{
				Type = ChildType_NonMature;
			}
		}
		// Not a sound wave node; try to find one in the child nodes. 
		else
		{
			// Iterate through all child nodes, looking for sound wave nodes. 
			for( INT ChildIndex = 0; ChildIndex < Node->ChildNodes.Num() ; ++ChildIndex )
			{
				USoundNode* ChildNode = Node->ChildNodes(ChildIndex);

				if( ChildNode ) 
				{
					// Found a sound wave node, sort it based on it's maturity level. 
					if( ChildNode->IsA( USoundNodeWave::StaticClass() ) )
					{
						USoundNodeWave* Wave = CastChecked<USoundNodeWave>(ChildNode);

						if( Wave->bMature )
						{
							Type = ChildType_Mature;
						}
						else
						{
							Type = ChildType_NonMature;
						}

						// Stop iterating as soon as we found the first sound wave. A possible problem 
						// here is that this function assumes there is only one sound wave per child output 
						// on the mature node. The worst case is that any additional child waves are never played. 
						break;
					}
					// Keep recursing over the children until a sound node waves is found. 
					else
					{
						Type = GetMaturityTypeForChild(ChildNode);
					}
				}
			}
		}
	}

	return Type;
}

/** 
 * This SoundNode uses UEngine::bAllowMatureLanguage to determine whether child nodes
 * that have USoundNodeWave::bMature=TRUE should be played.
 */
void USoundNodeMature::ParseNodes( UAudioDevice* AudioDevice, USoundNode* Parent, INT ChildIndex, UAudioComponent* AudioComponent, TArray<FWaveInstance*>& WaveInstances )
{
	RETRIEVE_SOUNDNODE_PAYLOAD( sizeof( INT ) );
	DECLARE_SOUNDNODE_ELEMENT( INT, NodeIndex );

	// A random sound node cannot use seamless looping as it might switch to another wave the next iteration.
	AudioComponent->CurrentNotifyOnLoop = FALSE;

	// Pick a random child node and save the index.
	if( *RequiresInitialization )
	{
		*RequiresInitialization = 0;

		// Make a list of mature and non-mature child nodes.
		TArray<INT> MatureChildNodes;
		MatureChildNodes.Empty( ChildNodes.Num() );

		TArray<INT> NonMatureChildNodes;
		NonMatureChildNodes.Empty( ChildNodes.Num() );

		for( INT i = 0; i < ChildNodes.Num() ; ++i )
		{
			if( ChildNodes( i ) ) 
			{
				EMaturityChildType Type = GetMaturityTypeForChild( ChildNodes( i ) );
				
				if( ChildType_Mature == Type )
				{
					MatureChildNodes.AddItem( i );
				}
				else if( ChildType_NonMature == Type )
				{
					NonMatureChildNodes.AddItem( i );
				}
				else
				{
					debugf( NAME_Warning, TEXT( "SoundNodeMature(%s) has a child which is not eventually linked to a sound node wave" ), *GetPathName() );
				}
			}
		}

		// Select a child node.
		NodeIndex = -1;
		if( GEngine->bAllowMatureLanguage )
		{
			// If mature language is allowed, prefer a mature node.
			if( MatureChildNodes.Num() > 0 )
			{
				NodeIndex = MatureChildNodes( 0 );
			}
			else if( NonMatureChildNodes.Num() > 0 )
			{
				NodeIndex = NonMatureChildNodes( 0 );
			}
		}
		else
		{
			// If mature language is not allowed, prefer a non-mature node.
			if( NonMatureChildNodes.Num() > 0 )
			{
				NodeIndex = NonMatureChildNodes( 0 );
			}
			else
			{
				debugf( NAME_Warning, TEXT( "SoundNodeMature(%s): GEngine->bAllowMatureLanguage is FALSE, no non-mature child sound exists" ), *GetPathName() );
			}
		}
	}

	// "play" the sound node that was selected
	if( NodeIndex >= 0 && NodeIndex < ChildNodes.Num() && ChildNodes( NodeIndex ) )
	{
		ChildNodes( NodeIndex )->ParseNodes( AudioDevice, this, NodeIndex, AudioComponent, WaveInstances );	
	}
}

/**
 * This sound node defaults to two child nodes
 * One for mature langauge and one for censored language
 */
void USoundNodeMature::CreateStartingConnectors( void )
{
	InsertChildNode( ChildNodes.Num() );
	InsertChildNode( ChildNodes.Num() );
}

/** 
 * Used to create a unique string to identify unique nodes
 */
FString USoundNodeMature::GetUniqueString( void )
{
	FString Unique = TEXT( "Mature" );

	Unique += TEXT( "/" );
	return( Unique );
}

/**
 * Called after object and all its dependencies have been serialized.
 */
void USoundNodeMature::PostLoad()
{
	Super::PostLoad();

	if( !GIsEditor && GEngine && !HasAnyFlags(RF_RootSet) && ChildNodes.Num() >= 2 )
	{
		TArray<INT> NodesToDelete;
		for( INT i = ChildNodes.Num() - 1; i >= 0 ; --i )
		{
			USoundNodeWave *Wave = Cast<USoundNodeWave>(ChildNodes(i));
			if (Wave != NULL && Wave->bMature != GEngine->bAllowMatureLanguage)
			{
				NodesToDelete.AddItem(i);
			}
		}

		if( NodesToDelete.Num() > 0 && NodesToDelete.Num() < ChildNodes.Num() )
		{
			for( INT i = 0; i < NodesToDelete.Num(); ++i )
			{
				ChildNodes.Remove(NodesToDelete(i));
			}
		}
	}
}

IMPLEMENT_CLASS( USoundNodeMature );

/*-----------------------------------------------------------------------------
USoundNodeEnveloper implementation.
-----------------------------------------------------------------------------*/

FString USoundNodeEnveloper::GetUniqueString()
{
	return FString(TEXT("Enveloper"));
}

void USoundNodeEnveloper::ParseNodes( UAudioDevice* AudioDevice, USoundNode* Parent, INT ChildIndex, class UAudioComponent* AudioComponent, TArray<FWaveInstance*>& WaveInstances )
{
	RETRIEVE_SOUNDNODE_PAYLOAD( sizeof( FLOAT ) );
	DECLARE_SOUNDNODE_ELEMENT( FLOAT, StartTime );

	if( *RequiresInitialization )
	{
		StartTime = AudioComponent->PlaybackTime;
		*RequiresInitialization = FALSE;
	}

	FLOAT PlayTime = AudioComponent->PlaybackTime - StartTime;

	if(bLoop && PlayTime > LoopEnd)
	{
		if( PlayTime > GetDuration() )
		{
			return;
		}

		FLOAT LoopDuration = LoopEnd - LoopStart;
		INT CurrentLoopCount = (INT)(PlayTime - LoopStart)/LoopDuration;
		PlayTime -= CurrentLoopCount*LoopDuration;

		if( CurrentLoopCount == LoopCount && !bLoopIndefinitely && LoopCount != 0 )
		{
			PlayTime += LoopDuration;
		}
	}

	if( VolumeInterpCurve != NULL )
	{
		AudioComponent->CurrentVolume *= VolumeInterpCurve->GetValue(PlayTime);
	}

	if( PitchInterpCurve != NULL )
	{
		AudioComponent->CurrentPitch *= PitchInterpCurve->GetValue(PlayTime);
	}

	Super::ParseNodes(AudioDevice, Parent, ChildIndex, AudioComponent, WaveInstances);
}

FLOAT USoundNodeEnveloper::GetDuration()
{
	FLOAT ChildDuration = (ChildNodes.Num() > 0 && ChildNodes(0) != NULL) ? ChildNodes(0)->GetDuration() : 0;

	if (bLoop && bLoopIndefinitely)
	{
		return INDEFINITELY_LOOPING_DURATION;
	}
	else if (bLoop)
	{
		return LoopStart + LoopCount*(LoopEnd-LoopStart) + DurationAfterLoop;
	}
	else
	{
		return ChildDuration;
	}
}

void USoundNodeEnveloper::PostEditChangeProperty( struct FPropertyChangedEvent& PropertyChangedEvent)
{
	// a few sanity checks
	if( LoopCount < 0 )
	{
		LoopCount = 0;
	}

	if( LoopEnd < LoopStart )
	{
		LoopEnd = LoopStart;
	}

	if( DurationAfterLoop < 0 )
	{
		DurationAfterLoop = 0;
	}
}

IMPLEMENT_CLASS( USoundNodeEnveloper );

/*-----------------------------------------------------------------------------
	USoundCue implementation.
-----------------------------------------------------------------------------*/

/** 
 * Calculate the maximum audible distance accounting for every node
 */
void USoundCue::CalculateMaxAudibleDistance( void )
{
	if( ( MaxAudibleDistance < SMALL_NUMBER ) && ( FirstNode != NULL ) )
	{
		// initialize AudibleDistance
		TArray<USoundNode*> SoundNodes;

		FirstNode->GetAllNodes( SoundNodes );
		for( INT i = 0; i < SoundNodes.Num(); i++ )
		{
			MaxAudibleDistance = SoundNodes( i )->MaxAudibleDistance( MaxAudibleDistance );
		}
		if( MaxAudibleDistance == 0.0f )
		{
			MaxAudibleDistance = WORLD_MAX;
		}
	}
}

UBOOL USoundCue::IsAudible( const FVector &SourceLocation, const FVector &ListenerLocation, AActor* SourceActor, INT& bIsOccluded, UBOOL bCheckOcclusion )
{
	//@fixme - naive implementation, needs to be optimized
	// for now, check max audible distance, and also if looping
	CalculateMaxAudibleDistance();

	AAmbientSoundSpline * AmbientSoundSpline = Cast<AAmbientSoundSpline>(SourceActor);
	if(NULL != AmbientSoundSpline)
	{
		USplineAudioComponent * SplineAudioComponent = Cast<USplineAudioComponent>(AmbientSoundSpline->AudioComponent);
		if(NULL != SplineAudioComponent)
		{
			const FLOAT TrueMaxDistance = Max(MaxAudibleDistance, SplineAudioComponent->ListenerScopeRadius);
			const FLOAT TrueMaxDistanceSq = TrueMaxDistance * TrueMaxDistance;
			//@notice Ranges are not handled
			//@notice Portals are not handled
			FLOAT MinimalDistanceSquared = BIG_NUMBER;
			for(INT PointIndex = 0; PointIndex < SplineAudioComponent->Points.Num(); PointIndex++)
			{
				const FVector& PointPosition = SplineAudioComponent->Points(PointIndex).Position;
				const FLOAT DistanceSq = ListenerLocation.DistanceSquared(PointPosition);
				MinimalDistanceSquared = Min(MinimalDistanceSquared, DistanceSq);
			}
			return (TrueMaxDistanceSq >= MinimalDistanceSquared);
		}
	}

	// Account for any portals
	FVector ModifiedSourceLocation = GWorld->GetWorldInfo()->RemapLocationThroughPortals( SourceLocation, ListenerLocation );

	if( MaxAudibleDistance * MaxAudibleDistance >= ( ListenerLocation - ModifiedSourceLocation ).SizeSquared() )
	{
		// Can't line check through portals
		if( bCheckOcclusion && ( MaxAudibleDistance != WORLD_MAX ) && ( ModifiedSourceLocation == SourceLocation ) )
		{
			// simple trace occlusion check - reduce max audible distance if occluded
			FCheckResult Hit( 1.0f );
			GWorld->SingleLineCheck( Hit, SourceActor, ListenerLocation, ModifiedSourceLocation, TRACE_World | TRACE_StopAtAnyHit );
			if( Hit.Time < 1.0f )
			{
				bIsOccluded = 1;
			}
			else
			{
				bIsOccluded = 0;
			}
		}

		return( TRUE );
	}
	else
	{
		return( FALSE );
	}
}

UBOOL USoundCue::IsAudibleSimple( FVector* Location )
{
	// No location means a range check is meaningless
	if( !Location )
	{
		return( TRUE );
	}

	// No audio device means no listeners to check against
	if( !GEngine || !GEngine->Client || !GEngine->Client->GetAudioDevice() )
	{
		return( TRUE );
	}

	// Listener position could change before long sounds finish
	if( GetCueDuration() > 1.0f )
	{
		return( TRUE );
	}

	// Calculate the MaxAudibleDistance from all the nodes
	CalculateMaxAudibleDistance();

	// Is this SourceActor within the MaxAudibleDistance of any of the listeners?
	UBOOL IsAudible = GEngine->Client->GetAudioDevice()->LocationIsAudible( *Location, MaxAudibleDistance );

	return( IsAudible );
}

FLOAT USoundCue::GetCueDuration( void )
{
	// Always recalc the duration when in the editor as it could change
	if( ( GIsEditor && !GIsGame ) || ( Duration == 0.0f ) )
	{
		if( FirstNode )
		{
			Duration = FirstNode->GetDuration();
		}
	}
	
	return( Duration );
}

IMPLEMENT_CLASS( USoundCue );

/*-----------------------------------------------------------------------------
	Parameter-based distributions
-----------------------------------------------------------------------------*/

UBOOL UDistributionFloatSoundParameter::GetParamValue( UObject* Data, FName ParamName, FLOAT& OutFloat )
{
	UBOOL bFoundParam = false;

	UAudioComponent* AudioComp = Cast<UAudioComponent>( Data );
	if( AudioComp )
	{
		bFoundParam = AudioComp->GetFloatParameter( ParameterName, OutFloat );
	}

	return( bFoundParam );
}

IMPLEMENT_CLASS( UDistributionFloatSoundParameter );

/** Hash function. Needed to avoid UObject v FResource ambiguity due to multiple inheritance */
DWORD GetTypeHash( const class USoundNodeWave* A )
{
	return( A ? A->GetIndex() : 0 );
}

// end

/*-----------------------------------------------------------------------------
         USoundNodeAttenuationAndGain implementation.
-----------------------------------------------------------------------------*/

void USoundNodeAttenuationAndGain::ParseNodes( UAudioDevice* AudioDevice, USoundNode* Parent, INT ChildIndex, class UAudioComponent* AudioComponent, TArray<FWaveInstance*>& WaveInstances )
{
	if( AudioComponent->bAllowSpatialization )
	{
		// Calculate distance from source to listener
		const FLOAT Distance = EvalDistance((ESoundDistanceCalc)DistanceType, AudioComponent);

		// Attenuate the volume based on the model
		if( bAttenuate )
		{
			if(Distance <= RadiusMin)
			{
				AudioComponent->CurrentVolume *= MinimalVolume;
			}
			else if(Distance < RadiusPeak)
			{
				const FLOAT ReverseDistance = RadiusPeak - Distance;
				const FLOAT ReverseRadiusMax = RadiusPeak - RadiusMin;
				AudioComponent->CurrentVolume *= MinimalVolume + (1.0f - MinimalVolume) * AttenuationEval(GainDistanceAlgorithm, ReverseDistance, 0.0f, ReverseRadiusMax, dBAttenuationAtMax);
			}
			else if(Distance < RadiusMax)
			{
				AudioComponent->CurrentVolume *= AttenuationEval(AttenuateDistanceAlgorithm, Distance, RadiusPeak, RadiusMax, dBAttenuationAtMax);
			}
			else
			{
				AudioComponent->CurrentVolume = 0;
			}
		}

		// Attenuate with the low pass filter if necessary
		if( bAttenuateWithLPF )
		{
			if( Distance <= LPFRadiusMin )
			{
				AudioComponent->CurrentHighFrequencyGain = LPFMinimal;
			}
			else if( Distance < LPFRadiusPeak )
			{
				AudioComponent->CurrentHighFrequencyGain = LPFMinimal + (1.0f - LPFMinimal) * ( ( Distance - LPFRadiusMin ) / ( LPFRadiusPeak - LPFRadiusMin ) );
			}
			else if( Distance < LPFRadiusMax )
			{
				AudioComponent->CurrentHighFrequencyGain = 1.0f - ((Distance - LPFRadiusPeak)/(LPFRadiusMax - LPFRadiusPeak));
			}
		}

		AudioComponent->CurrentUseSpatialization |= bSpatialize;
		AudioComponent->OmniRadius = OmniRadius;
	}
	else
	{
		AudioComponent->CurrentUseSpatialization = FALSE;
	}

	Super::ParseNodes( AudioDevice, Parent, ChildIndex, AudioComponent, WaveInstances );
}

FString USoundNodeAttenuationAndGain::GetUniqueString( )
{
	FString Unique = TEXT( "Attenuation and Gain" );

	if( bAttenuate )
	{
		Unique += FString::Printf( TEXT( " Vol %g %g %g %g" ), MinimalVolume, RadiusMin, RadiusPeak, RadiusMax );
	}
	if( bAttenuateWithLPF )
	{
		Unique += FString::Printf( TEXT( " Lpf %g %g %g %g" ), LPFMinimal, LPFRadiusMin, LPFRadiusPeak, LPFRadiusMax );
	}
	
	Unique += bSpatialize ? TEXT( " Spat" ) : TEXT( "" );
	Unique += FString::Printf( TEXT( " %d %d %d %g" ), GainDistanceAlgorithm, AttenuateDistanceAlgorithm, DistanceType, dBAttenuationAtMax );

	Unique += TEXT( "/" );
	return( Unique );
}

IMPLEMENT_CLASS( USoundNodeAttenuationAndGain );

/*-----------------------------------------------------------------------------
         UForcedLoopSoundNode implementation.
-----------------------------------------------------------------------------*/

/**
 * Notifies the sound node that a wave instance in its subtree has finished.
 *
 * @param WaveInstance	WaveInstance that was finished 
 */
UBOOL UForcedLoopSoundNode::NotifyWaveInstanceFinished( FWaveInstance* WaveInstance ) 
{
	USimpleSplineNonLoopAudioComponent * SimpleSplineNonLoopAudioComponent = Cast<USimpleSplineNonLoopAudioComponent>(WaveInstance->AudioComponent);
	if(NULL != SimpleSplineNonLoopAudioComponent)
	{
		SimpleSplineNonLoopAudioComponent->Reshuffle();
	}

	// Mark wave instance associated with wave as not yet finished.
	WaveInstance->bIsStarted = TRUE;
	WaveInstance->bIsFinished = FALSE;

	return( FALSE );
}

FLOAT UForcedLoopSoundNode::GetDuration( )
{
	return INDEFINITELY_LOOPING_DURATION;
}

FLOAT UForcedLoopSoundNode::MaxAudibleDistance( FLOAT CurrentMaxDistance ) 
{ 
	return( WORLD_MAX ); 
}

IMPLEMENT_CLASS( UForcedLoopSoundNode );


/*-----------------------------------------------------------------------------
         USoundNodeDoppler implementation.
-----------------------------------------------------------------------------*/

IMPLEMENT_CLASS( USoundNodeDoppler );

void USoundNodeDoppler::ParseNodes( UAudioDevice* AudioDevice, USoundNode* Parent, INT ChildIndex, UAudioComponent* AudioComponent, TArray<FWaveInstance*>& WaveInstances )
{
	if (AudioComponent)
	{
		AudioComponent->CurrentPitch *= GetDopplerPitchMultiplier(AudioDevice->Listeners(0), *AudioComponent);
	}

	Super::ParseNodes( AudioDevice, Parent, ChildIndex, AudioComponent, WaveInstances );
}

FLOAT USoundNodeDoppler::GetDopplerPitchMultiplier(FListener const& InListener, UAudioComponent const& AudioComponent) const
{
	static const FLOAT SpeedOfSoundInAirAtSeaLevel = 33000.f;		// cm/sec

	FVector const SourceToListenerNorm = (InListener.Location - AudioComponent.CurrentLocation).SafeNormal();

	// find source and listener speeds along the line between them
	FLOAT const SourceVelMagTorwardListener = AudioComponent.CurrentVelocity | SourceToListenerNorm;
	FLOAT const ListenerVelMagAwayFromSource = InListener.Velocity | SourceToListenerNorm;

	// multiplier = 1 / (1 - ((sourcevel - listenervel) / speedofsound) );
	FLOAT const InvDopplerPitchScale = 1.f - ( (SourceVelMagTorwardListener - ListenerVelMagAwayFromSource) / SpeedOfSoundInAirAtSeaLevel );
	FLOAT const PitchScale = 1.f / InvDopplerPitchScale;

	// factor in user-specified intensity
	FLOAT const FinalPitchScale = ((PitchScale - 1.f) * DopplerIntensity) + 1.f;

	//debugf(TEXT("Applying doppler pitchscale %f, raw scale %f, deltaspeed was %f"), FinalPitchScale, PitchScale, ListenerVelMagAwayFromSource - SourceVelMagTorwardListener);

	return FinalPitchScale;
}

FString USoundNodeDoppler::GetUniqueString( void )
{
	FString Unique = TEXT( "Doppler" );
	Unique += TEXT( "/" );
	return( Unique );
}

