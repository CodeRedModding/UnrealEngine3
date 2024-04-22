

/*------------------------------------------------------------------------------------
	Audio includes.
------------------------------------------------------------------------------------*/

#include "OpenSLAudioPrivate.h"
#include <dlfcn.h>

#define USE_OPENSL 1

SLInterfaceID SL_IID_ENGINE_Sym;
SLInterfaceID SL_IID_PLAY_Sym;
SLInterfaceID SL_IID_VOLUME_Sym;
SLInterfaceID SL_IID_BUFFERQUEUE_Sym;

SLresult SLAPIENTRY (*slCreateEngineFunc)(	SLObjectItf *pEngine,
											SLuint32 numOptions,
											const SLEngineOption *pEngineOptions,
											SLuint32 numInterfaces,
											const SLInterfaceID *pInterfaceIds,
											const SLboolean * pInterfaceRequired ) = NULL;

IMPLEMENT_CLASS( UOpenSLAudioDevice );

/**
 * Static constructor, used to associate .ini options with member variables.	
 */
void UOpenSLAudioDevice::StaticConstructor( void )
{
	new( GetClass(), TEXT( "DeviceName" ), RF_Public ) UStrProperty( CPP_PROPERTY( DeviceName ), TEXT( "OpenSLAudio" ), CPF_Config );
}

/**
 * Tears down audio device by stopping all sounds, removing all buffers, 
 * destroying all sources, ... Called by both Destroy and ShutdownAfterError
 * to perform the actual tear down.
 */
void UOpenSLAudioDevice::Teardown( void )
{
	// Flush stops all sources and deletes all buffers so sources can be safely deleted below.
	Flush( NULL );	

	// Destroy all sound sources
	for( INT i = 0; i < Sources.Num(); i++ )
	{
		delete Sources( i );
	}

	// CLOSE DOWN THE OPEN SL SYSTEM
}

//
//	UOpenSLAudioDevice::Serialize
//
void UOpenSLAudioDevice::Serialize( FArchive& Ar )
{
	Super::Serialize( Ar );

	if( Ar.IsCountingMemory() )
	{
		Ar.CountBytes( Buffers.Num() * sizeof( FOpenSLSoundBuffer ), Buffers.Num() * sizeof( FOpenSLSoundBuffer ) );
		Buffers.CountBytes( Ar );
		WaveBufferMap.CountBytes( Ar );
	}
}

/**
 * Shuts down audio device. This will never be called with the memory image codepath.
 */
void UOpenSLAudioDevice::FinishDestroy( void )
{
	if( !HasAnyFlags( RF_ClassDefaultObject ) )
	{
		Teardown();
		debugf( NAME_Exit, TEXT( "OpenSL Audio Device shut down." ) );
	}

	Super::FinishDestroy();
}

/**
 * Special variant of Destroy that gets called on fatal exit. Doesn't really
 * matter on the console so for now is just the same as Destroy so we can
 * verify that the code correctly cleans up everything.
 */
void UOpenSLAudioDevice::ShutdownAfterError( void )
{
	if( !HasAnyFlags( RF_ClassDefaultObject ) )
	{
		Teardown();
		debugf( NAME_Exit, TEXT( "UOpenSLAudioDevice::ShutdownAfterError" ) );
	}

	Super::ShutdownAfterError();
}


/**
 * Initializes the audio device and creates sources.
 *
 * @warning: 
 *
 * @return TRUE if initialization was successful, FALSE otherwise
 */
UBOOL UOpenSLAudioDevice::Init( void )
{
#if USE_OPENSL

	////////////
	// OpenSL //
	////////////

	extern void *GOPENSL_HANDLE;
	slCreateEngineFunc		= (SLresult (*)(const SLObjectItf_* const**, SLuint32, const SLEngineOption*, SLuint32, const SLInterfaceID_* const*, const SLboolean*)) dlsym(GOPENSL_HANDLE, "slCreateEngine");
	check( slCreateEngineFunc );

	SLInterfaceID* TempValue = NULL;

	TempValue = (SLInterfaceID*) dlsym(GOPENSL_HANDLE, "SL_IID_ENGINE");
	check( TempValue );
	SL_IID_ENGINE_Sym		= *TempValue;

	TempValue = (SLInterfaceID*) dlsym(GOPENSL_HANDLE, "SL_IID_PLAY");
	check( TempValue );
	SL_IID_PLAY_Sym			= *TempValue;

	TempValue = (SLInterfaceID*) dlsym(GOPENSL_HANDLE, "SL_IID_VOLUME");
	check( TempValue );
	SL_IID_VOLUME_Sym		= *TempValue;

	TempValue = (SLInterfaceID*) dlsym(GOPENSL_HANDLE, "SL_IID_BUFFERQUEUE");
	check( TempValue );
	SL_IID_BUFFERQUEUE_Sym	= *TempValue;

	SLresult result;
	
	SLEngineOption EngineOption[] = { (SLuint32) SL_ENGINEOPTION_THREADSAFE, (SLuint32) SL_BOOLEAN_TRUE};
		
    // create engine
    result = slCreateEngineFunc( &SL_EngineObject, 1, EngineOption, 0, NULL, NULL);
    check(SL_RESULT_SUCCESS == result);

    // realize the engine
    result = (*SL_EngineObject)->Realize(SL_EngineObject, SL_BOOLEAN_FALSE);
    check(SL_RESULT_SUCCESS == result);

    // get the engine interface, which is needed in order to create other objects
    result = (*SL_EngineObject)->GetInterface(SL_EngineObject, SL_IID_ENGINE_Sym, &SL_EngineEngine);
    check(SL_RESULT_SUCCESS == result);

	// create output mix, with environmental reverb specified as a non-required interface    
    result = (*SL_EngineEngine)->CreateOutputMix( SL_EngineEngine, &SL_OutputMixObject, 0, NULL, NULL );
    check(SL_RESULT_SUCCESS == result);

    // realize the output mix
    result = (*SL_OutputMixObject)->Realize(SL_OutputMixObject, SL_BOOLEAN_FALSE);
    check(SL_RESULT_SUCCESS == result);

	debugf(TEXT("SLEngine Initialized"));
    // ignore unsuccessful result codes for env

	////////////////
	// End OpenSL //
	////////////////
#endif
	
	// Default to sensible channel count.
	if( MaxChannels < 1 )
	{  
		MaxChannels = 12;
	}

	   
	// Initialize channels.
	for( INT i = 0; i < Min( MaxChannels, 12 ); i++ )
	{
		FOpenSLSoundSource* Source = new FOpenSLSoundSource( this );		
		Sources.AddItem( Source );
		FreeSources.AddItem( Source );
	}

	if( Sources.Num() < 1 )
	{
		debugf( NAME_Error,TEXT( "OpenSLAudio: couldn't allocate any sources" ) );
		return( FALSE );
	}

	// Update MaxChannels in case we couldn't create enough sources.
	MaxChannels = Sources.Num();
	debugf( NAME_Init, TEXT( "OpenSLAudioDevice: Allocated %i sources" ), MaxChannels );

	// Set up a default (nop) effects manager 
	Effects = new FAudioEffectsManager( this );

	// Initialized.
	NextResourceID = 1;

	// Initialize base class last as it's going to precache already loaded audio.
	Super::Init();

	return( TRUE );
}


void UOpenSLAudioDevice::Update( UBOOL Realtime )
{
	Super::Update( Realtime );

	// UDPATE LISTENERS
}

void UOpenSLAudioDevice::Precache( USoundNodeWave* Wave )
{
	FOpenSLSoundBuffer::Init( Wave, this );

	// If it's not native, then it will remain compressed
	INC_DWORD_STAT_BY( STAT_AudioMemorySize, Wave->RawData.GetBulkDataSize() );
	INC_DWORD_STAT_BY( STAT_AudioMemory, Wave->RawData.GetBulkDataSize() );
}


void UOpenSLAudioDevice::FreeResource( USoundNodeWave* SoundNodeWave )
{
	// Just in case the data was created but never uploaded
	if( SoundNodeWave->RawPCMData )
	{
		appFree( SoundNodeWave->RawPCMData );
		SoundNodeWave->RawPCMData = NULL;
	}

	// Find buffer for resident wavs
	if( SoundNodeWave->ResourceID )
	{
		// Find buffer associated with resource id.
		FOpenSLSoundBuffer* Buffer = WaveBufferMap.FindRef( SoundNodeWave->ResourceID );
		if( Buffer )
		{
			// Remove from buffers array.
			Buffers.RemoveItem( Buffer );

			// See if it is being used by a sound source...
			for( INT SrcIndex = 0; SrcIndex < Sources.Num(); SrcIndex++ )
			{
				FOpenSLSoundSource* Src = ( FOpenSLSoundSource* )( Sources( SrcIndex ) );
				if( Src && Src->Buffer && ( Src->Buffer == Buffer ) )
				{
					Src->Stop();
					break;
				}
			}

			delete Buffer;
		}

		SoundNodeWave->ResourceID = 0;
	}

	// .. or reference to compressed data
	SoundNodeWave->RemoveAudioResource();

	// Stat housekeeping
	DEC_DWORD_STAT_BY( STAT_AudioMemorySize, SoundNodeWave->GetResourceSize() );
	DEC_DWORD_STAT_BY( STAT_AudioMemory, SoundNodeWave->GetResourceSize() );
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// FOpenSLSoundSource
////////////////////////////////////////////////////////////////////////////////////////////////////

UBOOL FOpenSLSoundSource::Init( FWaveInstance* InWaveInstance )
{
	// don't do anything if no volume! THIS APPEARS TO HAVE THE VOLUME IN TIME, CHECK HERE THOUGH IF ISSUES
	if( InWaveInstance && ( InWaveInstance->Volume * InWaveInstance->VolumeMultiplier ) <= 0 )
	{
		return FALSE;
	}

	// Find matching buffer.
	Buffer = FOpenSLSoundBuffer::Init( InWaveInstance->WaveData, ( UOpenSLAudioDevice * )AudioDevice );
	if( Buffer && Buffer->GetSize() > 0 && InWaveInstance->WaveData->NumChannels <= 2 )
	{
		SCOPE_CYCLE_COUNTER( STAT_AudioSourceInitTime );		

#if USE_OPENSL
		////////////
		// OpenSL //
		////////////

		// data info
		SLDataLocator_AndroidSimpleBufferQueue LocationBuffer	= {		SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, 1 };
		
		debugf(TEXT("FOpenSLSoundSource::Init %s s:%d c:%d sr:%d"), *InWaveInstance->WaveData->GetPathName(), 
			Buffer->GetSize(), InWaveInstance->WaveData->NumChannels, InWaveInstance->WaveData->SampleRate );

		// PCM Info
		SLDataFormat_PCM PCM_Format				= {		SL_DATAFORMAT_PCM, InWaveInstance->WaveData->NumChannels, SLuint32( InWaveInstance->WaveData->SampleRate * 1000 ),	
														SL_PCMSAMPLEFORMAT_FIXED_16, SL_PCMSAMPLEFORMAT_FIXED_16, 
														InWaveInstance->WaveData->NumChannels == 2 ? ( SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT ) : SL_SPEAKER_FRONT_CENTER, 
														SL_BYTEORDER_LITTLEENDIAN };

		SLDataSource SoundDataSource			= {		&LocationBuffer, &PCM_Format};

		// configure audio sink
		SLDataLocator_OutputMix Output_Mix		= {		SL_DATALOCATOR_OUTPUTMIX, Device->SL_OutputMixObject};
		SLDataSink AudioSink					= {		&Output_Mix, NULL};
		
		// create audio player
		const SLInterfaceID	ids[] = {SL_IID_BUFFERQUEUE_Sym, SL_IID_VOLUME_Sym};
		const SLboolean		req[] = {SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE};
		SLresult result = (*Device->SL_EngineEngine)->CreateAudioPlayer( Device->SL_EngineEngine, &SL_PlayerObject, 
			&SoundDataSource, &AudioSink, sizeof(ids) / sizeof(SLInterfaceID), ids, req );
		if(result != SL_RESULT_SUCCESS) { debugf(TEXT("FAILED OPENSL BUFFER CreateAudioPlayer")); return FALSE; }

		UBOOL bFailedSetup = FALSE;

		 // realize the player
		result = (*SL_PlayerObject)->Realize(SL_PlayerObject, SL_BOOLEAN_FALSE);
		if(result != SL_RESULT_SUCCESS) { debugf(TEXT("FAILED OPENSL BUFFER Realize")); return FALSE; }

		// get the play interface
		result = (*SL_PlayerObject)->GetInterface(SL_PlayerObject, SL_IID_PLAY_Sym, &SL_PlayerPlayInterface);
		if(result != SL_RESULT_SUCCESS) { debugf(TEXT("FAILED OPENSL BUFFER GetInterface SL_IID_PLAY_Sym")); bFailedSetup |= TRUE; }
		// volume
		result = (*SL_PlayerObject)->GetInterface(SL_PlayerObject, SL_IID_VOLUME_Sym, &SL_VolumeInterface);
		if(result != SL_RESULT_SUCCESS) { debugf(TEXT("FAILED OPENSL BUFFER GetInterface SL_IID_VOLUME_Sym")); bFailedSetup |= TRUE; }
		// buffer system
		result = (*SL_PlayerObject)->GetInterface(SL_PlayerObject, SL_IID_BUFFERQUEUE_Sym, &SL_PlayerBufferQueue);
		if(result == SL_RESULT_SUCCESS) 
		{ 
			result = (*SL_PlayerBufferQueue)->Enqueue(SL_PlayerBufferQueue, Buffer->GetSoundData(), Buffer->GetSize() );
			if(result != SL_RESULT_SUCCESS) { debugf(TEXT("FAILED OPENSL BUFFER Enqueue SL_PlayerBufferQueue")); bFailedSetup |= TRUE; }
		}
		else
		{
			bFailedSetup |= TRUE; 
			debugf(TEXT("FAILED OPENSL BUFFER GetInterface SL_IID_BUFFERQUEUE_Sym")); 			
		}		

		// clean up the madness if anything we need failed
		if( bFailedSetup && SL_PlayerObject )
		{
			// close it down...
			(*SL_PlayerObject)->Destroy(SL_PlayerObject);			
			SL_PlayerObject			= NULL;
			SL_PlayerPlayInterface	= NULL;
			SL_PlayerBufferQueue	= NULL;
			SL_VolumeInterface		= NULL;
			return FALSE;
		}

		WaveInstance = InWaveInstance;

		////////////////
		// End OpenSL //
		////////////////
#endif

		Update();

		// Initialization was successful.
		return( TRUE );
	}

	// Failed to initialize source.
	return( FALSE );
}

/**
 * Clean up any hardware referenced by the sound source
 */
FOpenSLSoundSource::~FOpenSLSoundSource( void )
{
	// ?
}

/**
 * Updates the source specific parameter like e.g. volume and pitch based on the associated
 * wave instance.	
 */
void FOpenSLSoundSource::Update( void )
{
	SCOPE_CYCLE_COUNTER( STAT_AudioUpdateSources );

	if( !WaveInstance || Paused )
	{
		return;
	}

	FLOAT Volume = WaveInstance->Volume * WaveInstance->VolumeMultiplier;
	if( SetStereoBleed() )
	{
		// Emulate the bleed to rear speakers followed by stereo fold down
		Volume *= 1.25f;
	}

	Volume		= Clamp<FLOAT>( Volume, 0.0f, 1.0f );
	FLOAT Pitch = Clamp<FLOAT>( WaveInstance->Pitch, 0.4f, 2.0f );

	FVector Location;
	FVector	Velocity;

	// See file header for coordinate system explanation.
	Location.X = WaveInstance->Location.X;
	Location.Y = WaveInstance->Location.Z; // Z/Y swapped on purpose, see file header
	Location.Z = WaveInstance->Location.Y; // Z/Y swapped on purpose, see file header
	
	Velocity.X = WaveInstance->Velocity.X;
	Velocity.Y = WaveInstance->Velocity.Z; // Z/Y swapped on purpose, see file header
	Velocity.Z = WaveInstance->Velocity.Y; // Z/Y swapped on purpose, see file header

	// We're using a relative coordinate system for un- spatialized sounds.
	if( !WaveInstance->bUseSpatialization )
	{
		Location = FVector( 0.f, 0.f, 0.f );
	}

	// Set volume & Pitch
	// also Location & Velocity

	SLmillibel MaxMillibel = 0;
	SLmillibel MinMillibel = -3000;
	(*SL_VolumeInterface)->GetMaxVolumeLevel( SL_VolumeInterface, &MaxMillibel );

	// drop it down to an inaudible range
	if( Volume < 0.1f )
	{
		MinMillibel = -10000;
	}

	SLresult result = (*SL_VolumeInterface)->SetVolumeLevel(SL_VolumeInterface, ( Volume * ( MaxMillibel - MinMillibel ) ) + MinMillibel ); 
	check(SL_RESULT_SUCCESS == result);
}

/**
 * Plays the current wave instance.	
 */
void FOpenSLSoundSource::Play( void )
{
	if( WaveInstance )
	{
#if USE_OPENSL
		////////////
		// OpenSL //
		////////////

		// set the player's state to playing
		SLresult result = (*SL_PlayerPlayInterface)->SetPlayState(SL_PlayerPlayInterface, SL_PLAYSTATE_PLAYING);
		check(SL_RESULT_SUCCESS == result);

		////////////////
		// End OpenSL //
		////////////////
#endif
		Paused = FALSE;
		Playing = TRUE;
	}
}

/**
 * Stops the current wave instance and detaches it from the source.	
 */
void FOpenSLSoundSource::Stop( void )
{
	if( WaveInstance )
	{
#if USE_OPENSL
		////////////
		// OpenSL //
		////////////

		// set the player's state to stopped
		SLresult result = (*SL_PlayerPlayInterface)->SetPlayState(SL_PlayerPlayInterface, SL_PLAYSTATE_STOPPED);
		check(SL_RESULT_SUCCESS == result);
		
		// destroy file descriptor audio player object, and invalidate all associated interfaces
		if (SL_PlayerObject != NULL) 
		{
			(*SL_PlayerObject)->Destroy(SL_PlayerObject);			
			SL_PlayerObject			= NULL;
			SL_PlayerPlayInterface	= NULL;
			SL_PlayerBufferQueue	= NULL;
			SL_VolumeInterface		= NULL;
		}

		////////////////
		// End OpenSL //
		////////////////
#endif
		
		Paused = FALSE;
		Playing = FALSE;
		Buffer = NULL;
	}

	FSoundSource::Stop();
}

/**
 * Pauses playback of current wave instance.
 */
void FOpenSLSoundSource::Pause( void )
{
	if( WaveInstance )
	{
		Paused = TRUE;

#if USE_OPENSL
		////////////
		// OpenSL //
		////////////

		// set the player's state to paused
		SLresult result = (*SL_PlayerPlayInterface)->SetPlayState(SL_PlayerPlayInterface, SL_PLAYSTATE_PAUSED);
		check(SL_RESULT_SUCCESS == result);

		////////////////
		// End OpenSL //
		////////////////
#endif
	}
}

/** 
 * Returns TRUE if an OpenSL source has finished playing
 */
UBOOL FOpenSLSoundSource::IsSourceFinished( void )
{
#if USE_OPENSL
	////////////
	// OpenSL //
	////////////

	SLuint32 PlayState;

	// set the player's state to playing
	SLresult result = (*SL_PlayerPlayInterface)->GetPlayState(SL_PlayerPlayInterface, &PlayState);
	check(SL_RESULT_SUCCESS == result);

	if( PlayState == SL_PLAYSTATE_STOPPED )
	{
		return TRUE;
	}

	////////////////
	// End OpenSL //
	////////////////
#endif

	return FALSE;
}

/**
 * Queries the status of the currently associated wave instance.
 *
 * @return	TRUE if the wave instance/ source has finished playback and FALSE if it is 
 *			currently playing or paused.
 */
UBOOL FOpenSLSoundSource::IsFinished( void )
{
	if( WaveInstance )
	{
		// Check for a non starved, stopped source
		if( IsSourceFinished() )
		{


			// Notify the wave instance that it has finished playing.
			WaveInstance->NotifyFinished();
			return( TRUE );
		}
		else 
		{
			// is finished
		}

		return( FALSE );
	}

	return( TRUE );
}

/*------------------------------------------------------------------------------------
	FALSoundBuffer.
------------------------------------------------------------------------------------*/
/** 
 * Constructor
 *
 * @param AudioDevice	audio device this sound buffer is going to be attached to.
 */
FOpenSLSoundBuffer::FOpenSLSoundBuffer( UOpenSLAudioDevice* InAudioDevice )
{
	AudioDevice	= InAudioDevice;
	AudioData	= NULL;
}

/**
 * Destructor 
 * 
 * Frees wave data and detaches itself from audio device.
 */
FOpenSLSoundBuffer::~FOpenSLSoundBuffer( void )
{
	if( ResourceID )
	{
		AudioDevice->WaveBufferMap.Remove( ResourceID );
	}

	if( AudioData )
	{
		delete [] AudioData;
		AudioData = NULL;
	}
}

/**
 * Static function used to create a buffer.
 *
 * @param InWave		USoundNodeWave to use as template and wave source
 * @param AudioDevice	audio device to attach created buffer to
 * @param	bIsPrecacheRequest	Whether this request is for precaching or not
 * @return	FALSoundBuffer pointer if buffer creation succeeded, NULL otherwise
 */
FOpenSLSoundBuffer* FOpenSLSoundBuffer::Init( USoundNodeWave* Wave, UOpenSLAudioDevice* AudioDevice )
{
	SCOPE_CYCLE_COUNTER( STAT_AudioResourceCreationTime );

	// Can't create a buffer without any source data
	if( Wave == NULL || Wave->NumChannels == 0 )
	{
		return( NULL );
	}

	FOpenSLSoundBuffer* Buffer = NULL;

	// Find the existing buffer if any
	if( Wave->ResourceID )
	{
		Buffer = AudioDevice->WaveBufferMap.FindRef( Wave->ResourceID );
	}

	if( Buffer == NULL )
	{
		// Create new buffer.
		Buffer = new FOpenSLSoundBuffer( AudioDevice );

		// Allocate new resource ID and assign to USoundNodeWave. A value of 0 (default) means not yet registered.
		INT ResourceID = AudioDevice->NextResourceID++;
		Buffer->ResourceID = ResourceID;
		Wave->ResourceID = ResourceID;

		AudioDevice->Buffers.AddItem( Buffer );
		AudioDevice->WaveBufferMap.Set( ResourceID, Buffer );

		// Keep track of associated resource name.
		Buffer->ResourceName	= Wave->GetPathName();		
		Buffer->NumChannels		= Wave->NumChannels;
		Buffer->SampleRate		= Wave->SampleRate;

		// use the raw datra if it's allocated
		if (Wave->RawPCMData)
		{
			// upload it
			Buffer->BufferSize = Wave->RawPCMDataSize;

			// Free up the data if necessary
			if( Wave->bDynamicResource )
			{
				appFree( Wave->RawPCMData );
				Wave->RawPCMData = NULL;
				Wave->bDynamicResource = FALSE;
			}
		}
		else
		{
			// get the raw data
			BYTE* SoundData = ( BYTE* )Wave->RawData.Lock( LOCK_READ_ONLY );
			// it's (possibly) a pointer to a wave file, so skip over the header
			INT SoundDataSize = Wave->RawData.GetBulkDataSize();

			// is there a wave header?
			FWaveModInfo WaveInfo;
			if (WaveInfo.ReadWaveInfo(SoundData, SoundDataSize))
			{
				// if so, modify the location and size of the sound data based on header
				SoundData = WaveInfo.SampleDataStart;
				SoundDataSize = WaveInfo.SampleDataSize;
			}
			// let the Buffer know the final size
			Buffer->BufferSize = SoundDataSize;

			Buffer->AudioData = new BYTE[Buffer->BufferSize];
			appMemcpy( Buffer->AudioData, SoundData, Buffer->BufferSize );

			// unload it
			Wave->RawData.Unlock();
		}
	}

	return Buffer;
}
