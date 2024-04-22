/*=============================================================================
	ALAudioDevice.cpp: Unreal OpenAL Audio interface object.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

	Unreal is RHS with Y and Z swapped (or technically LHS with flipped axis)
=============================================================================*/

/*------------------------------------------------------------------------------------
	Audio includes.
------------------------------------------------------------------------------------*/

#include "ALAudioPrivate.h"

//     2 UU == 1"
// <=> 1 UU == 0.0127 m
#define AUDIO_DISTANCE_FACTOR ( 0.0127f )

#if _WINDOWS

// OpenAL function pointers
#define AL_EXT( name, strname ) UBOOL SUPPORTS##name;
#define AL_PROC( name, strname, ret, func, parms ) ret ( CDECL * func ) parms;
#include "ALFuncs.h"
#undef AL_EXT
#undef AL_PROC

#if SUPPORTS_PRAGMA_PACK
#pragma pack( push, 8 )
#endif

#include "UnConsoleTools.h"
#include "UnAudioDecompress.h"
#include <efx.h>

#if SUPPORTS_PRAGMA_PACK
#pragma pack( pop )
#endif

#elif PLATFORM_MACOSX

#include <vorbis/vorbisenc.h>
#include <vorbis/vorbisfile.h>
#include "UnConsoleTools.h"
#include "UnAudioDecompress.h"

#endif

/** Global variable to allow or disallow sound (used when app is in the background, it will not play sound) */
FLOAT GALGlobalVolumeMultiplier = 1.0f;

ALCdevice* UALAudioDevice::HardwareDevice = NULL;
ALCcontext*	UALAudioDevice::SoundContext = NULL;

/*------------------------------------------------------------------------------------
	UALAudioDevice constructor and UObject interface.
------------------------------------------------------------------------------------*/

IMPLEMENT_CLASS( UALAudioDevice );

/**
 * Static constructor, used to associate .ini options with member variables.	
 */
void UALAudioDevice::StaticConstructor( void )
{
	new( GetClass(), TEXT( "DeviceName" ), RF_Public ) UStrProperty( CPP_PROPERTY( DeviceName ), TEXT( "ALAudio" ), CPF_Config );
}

/**
 * Tears down audio device by stopping all sounds, removing all buffers, 
 * destroying all sources, ... Called by both Destroy and ShutdownAfterError
 * to perform the actual tear down.
 */
void UALAudioDevice::Teardown( void )
{
	// Flush stops all sources and deletes all buffers so sources can be safely deleted below.
	Flush( NULL );
	
	// Push any pending data to the hardware
	if( alcProcessContext )
	{	
		alcProcessContext( SoundContext );
	}

	// Destroy all sound sources
	for( INT i = 0; i < Sources.Num(); i++ )
	{
		delete Sources( i );
	}

	// Disable the context
	if( alcMakeContextCurrent )
	{	
		alcMakeContextCurrent( NULL );
	}

	// Destroy the context
	if( alcDestroyContext )
	{	
		alcDestroyContext( SoundContext );
		SoundContext = NULL;
	}

	// Close the hardware device
	if( alcCloseDevice )
	{	
		alcCloseDevice( HardwareDevice );
		HardwareDevice = NULL;
	}

	// Free up the OpenAL DLL
	if( DLLHandle )
	{
		appFreeDllHandle( DLLHandle );
		DLLHandle = NULL;
	}
}

//
//	UALAudioDevice::Serialize
//
void UALAudioDevice::Serialize( FArchive& Ar )
{
	Super::Serialize( Ar );

	if( Ar.IsCountingMemory() )
	{
		Ar.CountBytes( Buffers.Num() * sizeof( FALSoundBuffer ), Buffers.Num() * sizeof( FALSoundBuffer ) );
		Buffers.CountBytes( Ar );
		WaveBufferMap.CountBytes( Ar );
	}
}

/**
 * Shuts down audio device. This will never be called with the memory image codepath.
 */
void UALAudioDevice::FinishDestroy( void )
{
	if( !HasAnyFlags( RF_ClassDefaultObject ) )
	{
		Teardown();
		debugf( NAME_Exit, TEXT( "OpenAL Audio Device shut down." ) );
	}

	Super::FinishDestroy();
}

/**
 * Special variant of Destroy that gets called on fatal exit. Doesn't really
 * matter on the console so for now is just the same as Destroy so we can
 * verify that the code correctly cleans up everything.
 */
void UALAudioDevice::ShutdownAfterError( void )
{
	if( !HasAnyFlags( RF_ClassDefaultObject ) )
	{
		Teardown();
		debugf( NAME_Exit, TEXT( "UALAudioDevice::ShutdownAfterError" ) );
	}

	Super::ShutdownAfterError();
}

/*------------------------------------------------------------------------------------
	UAudioDevice Interface.
------------------------------------------------------------------------------------*/

#if IPHONE
ALCcontext* UALAudioDevice::ALAudioContext = NULL;
INT UALAudioDevice::SuspendCounter = 0;

/**
 * Initialize OpenAL early and in a thread to not delay startup by .66 seconds or so (on iPhone)
 */
void UALAudioDevice::IPhoneThreadedStaticInit()
{
	// Open device
	HardwareDevice = alcOpenDevice(NULL);
	checkf(HardwareDevice, TEXT("ALAudio: no OpenAL devices found."));

	// Create a context
	ALAudioContext = alcCreateContext(HardwareDevice, NULL);
	checkf(ALAudioContext, TEXT("ALAudio: context creation failed."));

	// make it the current context
	alcMakeContextCurrent(ALAudioContext);

	// mark the global SoundContext so that the main thread knows it's complete
	appInterlockedExchange((INT*)&SoundContext, (INT)(PTRINT)ALAudioContext);
}

void UALAudioDevice::ResumeContext()
{
	if (ALAudioContext != NULL)
	{
		alcMakeContextCurrent(ALAudioContext);
		alcProcessContext(ALAudioContext);
	}
	appInterlockedDecrement(&SuspendCounter);
}

void UALAudioDevice::SuspendContext()
{
	appInterlockedIncrement(&SuspendCounter);
	alcMakeContextCurrent(NULL);
	alcSuspendContext(ALAudioContext);
}

#endif

/**
 * Initializes the audio device and creates sources.
 *
 * @warning: 
 *
 * @return TRUE if initialization was successful, FALSE otherwise
 */
UBOOL UALAudioDevice::Init( void )
{
	// Make sure no interface classes contain any garbage
	Effects = NULL;
	DLLHandle = NULL;

	// Default to sensible channel count.
	if( MaxChannels < 1 )
	{
		MaxChannels = 32;
	}

	// Find DLL's.
#if _WINDOWS
	HardwareDevice = NULL;
	SoundContext = NULL;

	if( !DLLHandle )
	{
		DLLHandle = appGetDllHandle( AL_DLL );
		if( !DLLHandle )
		{
			debugf( NAME_Init, TEXT( "Couldn't locate %s - giving up." ), AL_DLL );
			return( FALSE );
		}
	}
	
	// Find functions.
	SUPPORTS_AL = TRUE;
	FindProcs( FALSE );
	if( !SUPPORTS_AL )
	{
		return( FALSE );
	}
#endif

#if IPHONE

	// make sure the other thread has initialized the sound context
	while (SoundContext == NULL)
	{
		appSleep(0.1f);
	}

#else

	// Open device
	HardwareDevice = alcOpenDevice( ( DeviceName.Len() > 0 ) ? TCHAR_TO_ANSI( *DeviceName ) : NULL );
	if( !HardwareDevice )
	{
		debugf( NAME_Init, TEXT( "ALAudio: no OpenAL devices found." ) );
		return( FALSE );
	}

	// Display the audio device that was actually opened
	const ALCchar* OpenedDeviceName = alcGetString( HardwareDevice, ALC_DEVICE_SPECIFIER );
	debugf( NAME_Init, TEXT( "ALAudio device requested : %s" ), *DeviceName );
	debugf( NAME_Init, TEXT( "ALAudio device opened    : %s" ), ANSI_TO_TCHAR( OpenedDeviceName ) );

	// Create a context
	INT Caps[] = 
	{ 
		ALC_FREQUENCY, 44100, 
#if _WINDOWS
		ALC_MAX_AUXILIARY_SENDS, 5, 
#endif
		ALC_STEREO_SOURCES, 4, 
		0, 0 };
	SoundContext = alcCreateContext( HardwareDevice, Caps );
	if( !SoundContext )
	{
		debugf( NAME_Init, TEXT( "ALAudio: context creation failed." ) );
		return( FALSE );
	}

	alcMakeContextCurrent( SoundContext );
	
#endif

	// Make sure everything happened correctly
	if( alError( TEXT( "Init" ) ) )
	{
		debugf( NAME_Init, TEXT( "ALAudio: alcMakeContextCurrent failed." ) );
		return( FALSE );
	}

	debugf( NAME_Init, TEXT( "AL_VENDOR      : %s" ), ANSI_TO_TCHAR( ( ANSICHAR* )alGetString( AL_VENDOR ) ) );
	debugf( NAME_Init, TEXT( "AL_RENDERER    : %s" ), ANSI_TO_TCHAR( ( ANSICHAR* )alGetString( AL_RENDERER ) ) );
	debugf( NAME_Init, TEXT( "AL_VERSION     : %s" ), ANSI_TO_TCHAR( ( ANSICHAR* )alGetString( AL_VERSION ) ) );
	debugf( NAME_Init, TEXT( "AL_EXTENSIONS  : %s" ), ANSI_TO_TCHAR( ( ANSICHAR* )alGetString( AL_EXTENSIONS ) ) );
 
	// Get the enums for multichannel support
	Surround40Format = alGetEnumValue( "AL_FORMAT_QUAD16" );
	Surround51Format = alGetEnumValue( "AL_FORMAT_51CHN16" );
	Surround61Format = alGetEnumValue( "AL_FORMAT_61CHN16" );
	Surround71Format = alGetEnumValue( "AL_FORMAT_71CHN16" );

	// Initialize channels.
	alError( TEXT( "Emptying error stack" ), 0 );
	for( INT i = 0; i < Min( MaxChannels, MAX_AUDIOCHANNELS ); i++ )
	{
		ALuint SourceId;
		alGenSources( 1, &SourceId );
		if( !alError( TEXT( "Init (creating sources)" ), 0 ) )
		{
			FALSoundSource* Source = new FALSoundSource( this );
			Source->SourceId = SourceId;
			Sources.AddItem( Source );
			FreeSources.AddItem( Source );
		}
		else
		{
			break;
		}
	}

	if( Sources.Num() < 1 )
	{
		debugf( NAME_Error,TEXT( "ALAudio: couldn't allocate any sources" ) );
		return( FALSE );
	}

	// Update MaxChannels in case we couldn't create enough sources.
	MaxChannels = Sources.Num();
	debugf( NAME_Init, TEXT( "ALAudioDevice: Allocated %i sources" ), MaxChannels );

	// Use our own distance model.
	alDistanceModel( AL_NONE );

	// Set up a default (nop) effects manager 
	Effects = new FAudioEffectsManager( this );

	// Initialized.
	NextResourceID = 1;

	// Initialize base class last as it's going to precache already loaded audio.
	Super::Init();

	return( TRUE );
}

/**
 * Update the audio device and calculates the cached inverse transform later
 * on used for spatialization.
 *
 * @param	Realtime	whether we are paused or not
 */
void UALAudioDevice::Update( UBOOL Realtime )
{
#if IPHONE
	if (SuspendCounter > 0)
	{
		return;
	}
#endif
	Super::Update( Realtime );

	// Set Player position
	FVector Location;

	// See file header for coordinate system explanation.
	Location.X = Listeners( 0 ).Location.X;
	Location.Y = Listeners( 0 ).Location.Z; // Z/Y swapped on purpose, see file header
	Location.Z = Listeners( 0 ).Location.Y; // Z/Y swapped on purpose, see file header
	Location *= AUDIO_DISTANCE_FACTOR;
	
	// Set Player orientation.
	FVector Orientation[2];

	// See file header for coordinate system explanation.
	Orientation[0].X = Listeners( 0 ).Front.X;
	Orientation[0].Y = Listeners( 0 ).Front.Z; // Z/Y swapped on purpose, see file header	
	Orientation[0].Z = Listeners( 0 ).Front.Y; // Z/Y swapped on purpose, see file header
	
	// See file header for coordinate system explanation.
	Orientation[1].X = Listeners( 0 ).Up.X;
	Orientation[1].Y = Listeners( 0 ).Up.Z; // Z/Y swapped on purpose, see file header
	Orientation[1].Z = Listeners( 0 ).Up.Y; // Z/Y swapped on purpose, see file header

	// Make the listener still and the sounds move relatively -- this allows 
	// us to scale the doppler effect on a per-sound basis.
	FVector Velocity = FVector( 0.0f, 0.0f, 0.0f );
	
	alListenerfv( AL_POSITION, ( ALfloat * )&Location );
	alListenerfv( AL_ORIENTATION, ( ALfloat * )&Orientation[0] );
	alListenerfv( AL_VELOCITY, ( ALfloat * )&Velocity );

	alError( TEXT( "UALAudioDevice::Update" ) );
}

/**
 * Precaches the passed in sound node wave object.
 *
 * @param	SoundNodeWave	Resource to be precached.
 */
void UALAudioDevice::Precache( USoundNodeWave* Wave )
{
	FALSoundBuffer::Init( Wave, this );

	// If it's not native, then it will remain compressed
	INC_DWORD_STAT_BY( STAT_AudioMemorySize, Wave->RawData.GetBulkDataSize() );
	INC_DWORD_STAT_BY( STAT_AudioMemory, Wave->RawData.GetBulkDataSize() );
}

/**
 * Frees the bulk resource data associated with this SoundNodeWave.
 *
 * @param	SoundNodeWave	wave object to free associated bulk data
 */
void UALAudioDevice::FreeResource( USoundNodeWave* SoundNodeWave )
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
		FALSoundBuffer* Buffer = WaveBufferMap.FindRef( SoundNodeWave->ResourceID );
		if( Buffer )
		{
			// Remove from buffers array.
			Buffers.RemoveItem( Buffer );

			// See if it is being used by a sound source...
			for( INT SrcIndex = 0; SrcIndex < Sources.Num(); SrcIndex++ )
			{
				FALSoundSource* Src = ( FALSoundSource* )( Sources( SrcIndex ) );
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

// sort memory usage from large to small unless bAlphaSort
static UBOOL bAlphaSort = FALSE;

IMPLEMENT_COMPARE_POINTER( FALSoundBuffer, ALAudioDevice, { 
	if( bAlphaSort == TRUE ) \
	{ \
		return( appStricmp( *A->ResourceName, *B->ResourceName ) ); \
	} \
	\
	return B->GetSize() - A->GetSize(); \
}
);

/** 
 * Displays debug information about the loaded sounds
 */
void UALAudioDevice::ListSounds( const TCHAR* Cmd, FOutputDevice& Ar )
{
	bAlphaSort = ParseParam( Cmd, TEXT( "ALPHASORT" ) );

	INT	TotalSoundSize = 0;

	Ar.Logf( TEXT( "Sound resources:" ) );

	TArray<FALSoundBuffer*> AllSounds = Buffers;

	Sort<USE_COMPARE_POINTER( FALSoundBuffer, ALAudioDevice )>( &AllSounds( 0 ), AllSounds.Num() );

	for( INT i = 0; i < AllSounds.Num(); ++i )
	{
		FALSoundBuffer* Buffer = AllSounds(i);
		Ar.Logf( TEXT( "RawData: %8.2f Kb (%d channels at %d Hz) in sound %s" ), Buffer->GetSize() / 1024.0f, Buffer->GetNumChannels(), Buffer->SampleRate, *Buffer->ResourceName );
		TotalSoundSize += Buffer->GetSize();
	}

	Ar.Logf( TEXT( "%8.2f Kb for %d sounds" ), TotalSoundSize / 1024.0f, AllSounds.Num() );
}

ALuint UALAudioDevice::GetInternalFormat( INT NumChannels )
{
	ALuint InternalFormat = 0;

	switch( NumChannels )
	{
	case 0:
	case 3:
	case 5:
		break;
	case 1:
		InternalFormat = AL_FORMAT_MONO16;
		break;
	case 2:
		InternalFormat = AL_FORMAT_STEREO16;
		break;
	case 4:
		InternalFormat = Surround40Format;
		break;
	case 6:
		InternalFormat = Surround51Format;
		break;	
	case 7:
		InternalFormat = Surround61Format;
		break;	
	case 8:
		InternalFormat = Surround71Format;
		break;
	}

	return( InternalFormat );
}

/*------------------------------------------------------------------------------------
OpenAL utility functions
------------------------------------------------------------------------------------*/
//
//	FindExt
//
UBOOL UALAudioDevice::FindExt( const TCHAR* Name )
{
	if( alIsExtensionPresent( TCHAR_TO_ANSI( Name ) ) || alcIsExtensionPresent( HardwareDevice, TCHAR_TO_ANSI( Name ) ) )
	{
		debugf( NAME_Init, TEXT( "Device supports: %s" ), Name );
		return( TRUE );
	}

	return( FALSE );
}

//
//	FindProc
//
void UALAudioDevice::FindProc( void*& ProcAddress, char* Name, char* SupportName, UBOOL& Supports, UBOOL AllowExt )
{
	ProcAddress = NULL;
	if( !ProcAddress )
	{
		ProcAddress = appGetDllExport( DLLHandle, ANSI_TO_TCHAR( Name ) );
	}
	if( !ProcAddress && Supports && AllowExt )
	{
		ProcAddress = alGetProcAddress( ( ALchar * ) Name );
	}
	if( !ProcAddress )
	{
		if( Supports )
		{
			debugf( TEXT("   Missing function '%s' for '%s' support"), ANSI_TO_TCHAR( Name ), ANSI_TO_TCHAR( SupportName ) );
		}
		Supports = FALSE;
	}
}

//
//	FindProcs
//
void UALAudioDevice::FindProcs( UBOOL AllowExt )
{

#if _WINDOWS

#define AL_EXT( name, strname ) if( AllowExt ) SUPPORTS##name = FindExt( TEXT( #strname ) );
#define AL_PROC( name, strname, ret, func, parms ) FindProc( *( void ** )&func, #func, #strname, SUPPORTS##name, AllowExt );
	#include "ALFuncs.h"
#undef AL_EXT
#undef AL_PROC

#endif

}

//
//	alError
//
UBOOL UALAudioDevice::alError( const TCHAR* Text, UBOOL Log )
{
	ALenum Error = alGetError();
	if( Error != AL_NO_ERROR )
	{
		do 
		{		
			if( Log )
			{
				switch ( Error )
				{
				case AL_INVALID_NAME:
					debugf( TEXT( "ALAudio: AL_INVALID_NAME in %s" ), Text );
					break;
				case AL_INVALID_VALUE:
					debugf( TEXT( "ALAudio: AL_INVALID_VALUE in %s" ), Text );
					break;
				case AL_OUT_OF_MEMORY:
					debugf( TEXT( "ALAudio: AL_OUT_OF_MEMORY in %s" ), Text );
					break;
				case AL_INVALID_ENUM:
					debugf( TEXT( "ALAudio: AL_INVALID_ENUM in %s" ), Text );
					break;
				case AL_INVALID_OPERATION:
					debugf( TEXT( "ALAudio: AL_INVALID_OPERATION in %s" ), Text );
					break;
				default:
					debugf( TEXT( "ALAudio: Unknown error in %s" ), Text );
					break;
				}
			}
		}
		while( ( Error = alGetError() ) != AL_NO_ERROR );

		return( TRUE );
	}

	return( FALSE );
}

/*------------------------------------------------------------------------------------
	FALSoundSource.
------------------------------------------------------------------------------------*/

/**
 * Initializes a source with a given wave instance and prepares it for playback.
 *
 * @param	WaveInstance	wave instace being primed for playback
 * @return	TRUE if initialization was successful, FALSE otherwise
 */
UBOOL FALSoundSource::Init( FWaveInstance* InWaveInstance )
{
	// Find matching buffer.
	Buffer = FALSoundBuffer::Init( InWaveInstance->WaveData, ( UALAudioDevice * )AudioDevice );
	if( Buffer )
	{
		SCOPE_CYCLE_COUNTER( STAT_AudioSourceInitTime );

		WaveInstance = InWaveInstance;

		// Enable/disable spatialisation of sounds
		alSourcei( SourceId, AL_SOURCE_RELATIVE, WaveInstance->bUseSpatialization ? AL_FALSE : AL_TRUE );

		// Setting looping on a real time decompressed source suppresses the buffers processed message
		alSourcei( SourceId, AL_LOOPING, ( WaveInstance->LoopingMode == LOOP_Forever ) ? AL_TRUE : AL_FALSE );

		// Always queue up the first buffer
		alSourceQueueBuffers( SourceId, 1, Buffer->BufferIds );	
		if( WaveInstance->LoopingMode == LOOP_WithNotification )
		{		
			// We queue the sound twice for wave instances that use seamless looping so we can have smooth 
			// loop transitions. The downside is that we might play at most one frame worth of audio from the 
			// beginning of the wave after the wave stops looping.
			alSourceQueueBuffers( SourceId, 1, Buffer->BufferIds );
		}

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
FALSoundSource::~FALSoundSource( void )
{
	// @todo openal: What do we do here
	/// AudioDevice->DestroyEffect( this );

	alDeleteSources( 1, &SourceId );
}

/**
 * Updates the source specific parameter like e.g. volume and pitch based on the associated
 * wave instance.	
 */
void FALSoundSource::Update( void )
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

	Volume = Clamp<FLOAT>( Volume, 0.0f, 1.0f );
	FLOAT Pitch = Clamp<FLOAT>( WaveInstance->Pitch, 0.4f, 2.0f );

	// apply global multiplier (ie to disable sound when not the foreground app)
	Volume *= GALGlobalVolumeMultiplier;

	// Set whether to apply reverb
	SetReverbApplied( TRUE );

	// Set the HighFrequencyGain value
	SetHighFrequencyGain();

	FVector Location;
	FVector	Velocity;

	// See file header for coordinate system explanation.
	Location.X = WaveInstance->Location.X;
	Location.Y = WaveInstance->Location.Z; // Z/Y swapped on purpose, see file header
	Location.Z = WaveInstance->Location.Y; // Z/Y swapped on purpose, see file header
	
	Velocity.X = WaveInstance->Velocity.X;
	Velocity.Y = WaveInstance->Velocity.Z; // Z/Y swapped on purpose, see file header
	Velocity.Z = WaveInstance->Velocity.Y; // Z/Y swapped on purpose, see file header

	// Convert to meters.
	Location *= AUDIO_DISTANCE_FACTOR;
	Velocity *= AUDIO_DISTANCE_FACTOR;

	// We're using a relative coordinate system for un- spatialized sounds.
	if( !WaveInstance->bUseSpatialization )
	{
		Location = FVector( 0.f, 0.f, 0.f );
	}

	alSourcef( SourceId, AL_GAIN, Volume );	
	alSourcef( SourceId, AL_PITCH, Pitch );		

	alSourcefv( SourceId, AL_POSITION, ( ALfloat * )&Location );
	alSourcefv( SourceId, AL_VELOCITY, ( ALfloat * )&Velocity );

	// Platform dependent call to update the sound output with new parameters
	// @todo openal: Is this no longer needed?
	/// AudioDevice->UpdateEffect( this );
}

/**
 * Plays the current wave instance.	
 */
void FALSoundSource::Play( void )
{
	if( WaveInstance )
	{
		alSourcePlay( SourceId );
		Paused = FALSE;
		Playing = TRUE;
	}
}

/**
 * Stops the current wave instance and detaches it from the source.	
 */
void FALSoundSource::Stop( void )
{
	if( WaveInstance )
	{
		alSourceStop( SourceId );
		// This clears out any pending buffers that may or may not be queued or played
		alSourcei( SourceId, AL_BUFFER, 0 );
		Paused = FALSE;
		Playing = FALSE;
		Buffer = NULL;
	}

	FSoundSource::Stop();
}

/**
 * Pauses playback of current wave instance.
 */
void FALSoundSource::Pause( void )
{
	if( WaveInstance )
	{
		alSourcePause( SourceId );
		Paused = TRUE;
	}
}

/** 
 * Returns TRUE if an OpenAL source has finished playing
 */
UBOOL FALSoundSource::IsSourceFinished( void )
{
	ALint State = AL_STOPPED;

	// Check the source for data to continue playing
	alGetSourcei( SourceId, AL_SOURCE_STATE, &State );
	if( State == AL_PLAYING || State == AL_PAUSED )
	{
		return( FALSE );
	}

	return( TRUE );
}

/** 
 * Handle dequeuing and requeuing of a single buffer
 */
void FALSoundSource::HandleQueuedBuffer( void )
{
	ALuint	DequeuedBuffer;

	// Unqueue the processed buffers
	alSourceUnqueueBuffers( SourceId, 1, &DequeuedBuffer );

	// Notify the wave instance that the current (native) buffer has finished playing.
	WaveInstance->NotifyFinished();

	// Queue the same packet again for looping
	alSourceQueueBuffers( SourceId, 1, Buffer->BufferIds );
}

/**
 * Queries the status of the currently associated wave instance.
 *
 * @return	TRUE if the wave instance/ source has finished playback and FALSE if it is 
 *			currently playing or paused.
 */
UBOOL FALSoundSource::IsFinished( void )
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
			// Check to see if any complete buffers have been processed
			ALint BuffersProcessed;
			alGetSourcei( SourceId, AL_BUFFERS_PROCESSED, &BuffersProcessed );

			switch( BuffersProcessed )
			{
			case 0:
				// No buffers need updating
				break;

			case 1:
				// Standard case of 1 buffer expired which needs repopulating
				HandleQueuedBuffer();
				break;

			case 2:
				// Starvation case when the source has stopped 
				HandleQueuedBuffer();
				HandleQueuedBuffer();

				// Restart the source
				alSourcePlay( SourceId );
				break;
			}
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
FALSoundBuffer::FALSoundBuffer( UALAudioDevice* InAudioDevice )
{
	AudioDevice	= InAudioDevice;
}

/**
 * Destructor 
 * 
 * Frees wave data and detaches itself from audio device.
 */
FALSoundBuffer::~FALSoundBuffer( void )
{
	if( ResourceID )
	{
		AudioDevice->WaveBufferMap.Remove( ResourceID );
	}

	// Delete AL buffers.
	alDeleteBuffers( 1, BufferIds );
}

/**
 * Static function used to create a buffer.
 *
 * @param InWave		USoundNodeWave to use as template and wave source
 * @param AudioDevice	audio device to attach created buffer to
 * @param	bIsPrecacheRequest	Whether this request is for precaching or not
 * @return	FALSoundBuffer pointer if buffer creation succeeded, NULL otherwise
 */
FALSoundBuffer* FALSoundBuffer::Init( USoundNodeWave* Wave, UALAudioDevice* AudioDevice )
{
	SCOPE_CYCLE_COUNTER( STAT_AudioResourceCreationTime );

	// Can't create a buffer without any source data
	if( Wave == NULL || Wave->NumChannels == 0 )
	{
		return( NULL );
	}

	FALSoundBuffer* Buffer = NULL;

	// Find the existing buffer if any
	if( Wave->ResourceID )
	{
		Buffer = AudioDevice->WaveBufferMap.FindRef( Wave->ResourceID );
	}

	if( Buffer == NULL )
	{
		// Create new buffer.
		Buffer = new FALSoundBuffer( AudioDevice );

		alGenBuffers( 1, Buffer->BufferIds );

		AudioDevice->alError( TEXT( "RegisterSound" ) );

		// Allocate new resource ID and assign to USoundNodeWave. A value of 0 (default) means not yet registered.
		INT ResourceID = AudioDevice->NextResourceID++;
		Buffer->ResourceID = ResourceID;
		Wave->ResourceID = ResourceID;

		AudioDevice->Buffers.AddItem( Buffer );
		AudioDevice->WaveBufferMap.Set( ResourceID, Buffer );

		// Keep track of associated resource name.
		Buffer->ResourceName = Wave->GetPathName();

		Buffer->InternalFormat = AudioDevice->GetInternalFormat( Wave->NumChannels );		
		Buffer->NumChannels = Wave->NumChannels;
		Buffer->SampleRate = Wave->SampleRate;

#if PLATFORM_MACOSX
		if( !Wave->RawPCMData && Wave->CompressedPCData.GetBulkDataSize() )
		{
			// Gotta decompress OGG data - it looks like no one will do this for us

			FVorbisAudioInfo	OggInfo;
			FSoundQualityInfo	QualityInfo = { 0 };

			BYTE* OggData = ( BYTE* )Wave->CompressedPCData.Lock( LOCK_READ_ONLY );

			// Parse the ogg vorbis header for the relevant information
			if( OggInfo.ReadCompressedInfo( OggData, Wave->CompressedPCData.GetBulkDataSize(), &QualityInfo ) )
			{
				// Extract the data
				Wave->SampleRate = QualityInfo.SampleRate;
				Wave->NumChannels = QualityInfo.NumChannels;
				Wave->Duration = QualityInfo.Duration;

				Wave->RawPCMDataSize = QualityInfo.SampleDataSize;
				Wave->RawPCMData = ( BYTE* )appMalloc( Wave->RawPCMDataSize );

				// Decompress all the sample data into preallocated memory
				OggInfo.ExpandFile( Wave->RawPCMData, &QualityInfo );
			}

			Wave->CompressedPCData.Unlock();

			if( Wave->RawPCMData )
			{
				Wave->CompressedPCData.RemoveBulkData();
			}
		}
#endif

		// use the raw datra if it's allocated
		if (Wave->RawPCMData)
		{
			// upload it
			Buffer->BufferSize = Wave->RawPCMDataSize;
			alBufferData( Buffer->BufferIds[0], Buffer->InternalFormat, Wave->RawPCMData, Wave->RawPCMDataSize, Buffer->SampleRate );

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

			// upload it
			alBufferData( Buffer->BufferIds[0], Buffer->InternalFormat, SoundData, Buffer->BufferSize, Buffer->SampleRate );
			// unload it
			Wave->RawData.Unlock();
		}

		if( AudioDevice->alError( TEXT( "RegisterSound (buffer data)" ) ) || ( Buffer->BufferSize == 0 ) )
		{
			Buffer->InternalFormat = 0;
		}

		if( Buffer->InternalFormat == 0 )
		{
			debugf( NAME_Warning, TEXT( "Audio: sound format not supported for '%s' (%d)" ), *Wave->GetName(), Wave->NumChannels );
			delete Buffer;
			Buffer = NULL;
		}
	}

	return Buffer;
}
