/*=============================================================================
	XeAudioDevice.cpp: Unreal XAudio2 Audio interface object.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

	Unreal is RHS with Y and Z swapped (or technically LHS with flipped axis)

=============================================================================*/

/*------------------------------------------------------------------------------------
	Audio includes.
------------------------------------------------------------------------------------*/

#include "Engine.h"

#include <xapobase.h>
#include <xapofx.h>
#include <xaudio2fx.h>

#include "EngineSoundClasses.h"
#include "EngineAudioDeviceClasses.h"
#include "UnAudioEffect.h"
#include "UnConsoleTools.h"
#include "UnAudioDecompress.h"
#include "XAudio2Device.h"
#include "XAudio2Effects.h"

/*------------------------------------------------------------------------------------
	Static variables from the early init
------------------------------------------------------------------------------------*/

// The number of speakers producing sound (stereo or 5.1)
INT UXAudio2Device::NumSpeakers							= 0;
IXAudio2* UXAudio2Device::XAudio2						= NULL;
const FLOAT* UXAudio2Device::OutputMixMatrix				= NULL;
IXAudio2MasteringVoice* UXAudio2Device::MasteringVoice	= NULL;
XAUDIO2_DEVICE_DETAILS UXAudio2Device::DeviceDetails		= { 0 };

/*------------------------------------------------------------------------------------
	UXAudio2Device constructor and UObject interface.
------------------------------------------------------------------------------------*/

IMPLEMENT_CLASS( UXAudio2Device );

/**
 * Static constructor, used to associate .ini options with member variables.	
 */
void UXAudio2Device::StaticConstructor( void )
{
}

/**  
 * Check for errors and output a human readable string 
 */
UBOOL UXAudio2Device::ValidateAPICall( const TCHAR* Function, INT ErrorCode )
{
	if( ErrorCode != S_OK )
	{
		switch( ErrorCode )
		{
		case XAUDIO2_E_INVALID_CALL:
			debugf( NAME_DevAudio, TEXT( "%s error: Invalid Call" ), Function );
			break;

		case XAUDIO2_E_XMA_DECODER_ERROR:
			debugf( NAME_DevAudio, TEXT( "%s error: XMA Decoder Error" ), Function );
			break;

		case XAUDIO2_E_XAPO_CREATION_FAILED:
			debugf( NAME_DevAudio, TEXT( "%s error: XAPO Creation Failed" ), Function );
			break;

		case XAUDIO2_E_DEVICE_INVALIDATED:
			debugf( NAME_DevAudio, TEXT( "%s error: Device Invalidated" ), Function );
			break;
		};
		return( FALSE );
	}

	return( TRUE );
}

/**
 * Tears down audio device by stopping all sounds, removing all buffers, 
 * destroying all sources, ... Called by both Destroy and ShutdownAfterError
 * to perform the actual tear down.
 */
void UXAudio2Device::Teardown( void )
{
	// Flush stops all sources so sources can be safely deleted below.
	Flush( NULL );

	// Release any loaded buffers - this calls stop on any sources that need it
	for( INT i = Buffers.Num() - 1; i >= 0; i-- )
	{
		FXAudio2SoundBuffer* Buffer = Buffers( i );
		FreeBufferResource( Buffer );
	}

	// Must be after FreeBufferResource as that potentially stops sources
	for( INT i = 0; i < Sources.Num(); i++ )
	{
		delete Sources( i );
	}

	// Clear out the EQ/Reverb/LPF effects
	delete Effects;

	Sources.Empty();
	FreeSources.Empty();

	if( MasteringVoice )
	{
		MasteringVoice->DestroyVoice();
		MasteringVoice = NULL;
	}

	if( XAudio2 )
	{
		// Force the hardware to release all references
		XAudio2->Release();
		XAudio2 = NULL;
	}

#if _WINDOWS
	CoUninitialize();
	GIsCOMInitialized = FALSE;
#endif
}

/**
 * Shuts down audio device. This will never be called with the memory image
 * codepath.
 */
void UXAudio2Device::FinishDestroy( void )
{
	if( !HasAnyFlags( RF_ClassDefaultObject ) )
	{
		Teardown();
		debugf( NAME_Exit, TEXT( "XAudio2 Device shut down." ) );
	}

	Super::FinishDestroy();
}

/**
 * Special variant of Destroy that gets called on fatal exit. Doesn't really
 * matter on the console so for now is just the same as Destroy so we can
 * verify that the code correctly cleans up everything.
 */
void UXAudio2Device::ShutdownAfterError( void )
{
	if( !HasAnyFlags( RF_ClassDefaultObject ) )
	{
		debugf( NAME_Exit, TEXT( "UXAudio2Device::ShutdownAfterError" ) );
	}

	Super::ShutdownAfterError();
}

/*------------------------------------------------------------------------------------
	UAudioDevice Interface.
------------------------------------------------------------------------------------*/

/**
 * Initializes the audio device and creates sources.
 *
 * @warning: Relies on XAudioInitialize already being called
 *
 * @return TRUE if initialization was successful, FALSE otherwise
 */
UBOOL UXAudio2Device::Init( void )
{
#if DEDICATED_SERVER
	return FALSE;
#endif
	// Make sure no interface classes contain any garbage
	Effects = NULL;

	// Default to sensible channel count.
	if( MaxChannels < 1 )
	{
		MaxChannels = 32;
	}

	// Init everything in XAudio2
	if( !InitHardware() )
	{
		return( FALSE );
	}

	// Create the effects subsystem (reverb, EQ, etc.)
	Effects = new FXAudio2EffectsManager( this );

	// Initialize channels.
	for( INT i = 0; i < Min( MaxChannels, MAX_AUDIOCHANNELS ); i++ )
	{
		FXAudio2SoundSource* Source = new FXAudio2SoundSource( this, Effects );
		Sources.AddItem( Source );
		FreeSources.AddItem( Source );
	}

	if( !Sources.Num() )
	{
		debugf( NAME_Error, TEXT( "XAudio2Device: couldn't allocate sources" ) );
		return FALSE;
	}

	// Update MaxChannels in case we couldn't create enough sources.
	MaxChannels = Sources.Num();
	debugf( NAME_DevAudio, TEXT( "Allocated %i sources" ), MaxChannels );

	// Initialize permanent memory stack for initial & always loaded sound allocations.
	if( CommonAudioPoolSize )
	{
		debugf( NAME_DevAudio, TEXT( "Allocating %g MByte for always resident audio data" ), CommonAudioPoolSize / ( 1024.0f * 1024.0f ) );
		CommonAudioPoolFreeBytes = CommonAudioPoolSize;
		CommonAudioPool = ( BYTE* )appPhysicalAlloc( CommonAudioPoolSize, CACHE_Normal );
	}
	else
	{
		debugf( NAME_DevAudio, TEXT( "CommonAudioPoolSize is set to 0 - disabling persistent pool for audio data" ) );
		CommonAudioPoolFreeBytes = 0;
	}

	// Initialized.
	NextResourceID = 1;

	// Initialize base class last as it's going to precache already loaded audio.
	Super::Init();

	return( TRUE );
}

/** 
 * Simple init of XAudio2 device for Bink audio
 */
UBOOL UXAudio2Device::InitHardware( void )
{
	if( XAudio2 == NULL )
	{
		UINT32 SampleRate = 0;

#if _WINDOWS
		if( !GIsCOMInitialized )
		{
			CoInitialize( NULL );
			GIsCOMInitialized = TRUE;
		}
#endif

#if _WIN64
		// Work around the fact the x64 version of XAudio2_7.dll does not properly ref count
		// by forcing it to be always loaded
		LoadLibraryA( "XAudio2_7.dll" );
#endif

		UINT32 Flags = 0;	// XAUDIO2_DEBUG_ENGINE;
		if( XAudio2Create( &XAudio2, Flags, AUDIO_HWTHREAD ) != S_OK )
		{
			debugf( NAME_Init, TEXT( "Failed to create XAudio2 interface" ) );
			return( FALSE );
		}

		UINT32 DeviceCount = 0;
		XAudio2->GetDeviceCount( &DeviceCount );
		if( DeviceCount < 1 )
		{
			debugf( NAME_Init, TEXT( "No audio devices found!" ) );
			XAudio2 = NULL;
			return( FALSE );		
		}

		// Get the details of the default device 0
		if( XAudio2->GetDeviceDetails( 0, &DeviceDetails ) != S_OK )
		{
			debugf( NAME_Init, TEXT( "Failed to get DeviceDetails for XAudio2" ) );
			XAudio2 = NULL;
			return( FALSE );
		}

		NumSpeakers = DeviceDetails.OutputFormat.Format.nChannels;
		SampleRate = DeviceDetails.OutputFormat.Format.nSamplesPerSec;

		// Clamp the output frequency to the limits of the reverb XAPO
		if( SampleRate > XAUDIO2FX_REVERB_MAX_FRAMERATE )
		{
			SampleRate = XAUDIO2FX_REVERB_MAX_FRAMERATE;
			DeviceDetails.OutputFormat.Format.nSamplesPerSec = SampleRate;
		}

		debugf( NAME_Init, TEXT( "XAudio2 using '%s' : %d channels at %g kHz using %d bits per sample (channel mask 0x%x)" ), 
			DeviceDetails.DisplayName,
			NumSpeakers, 
			( FLOAT )SampleRate / 1000.0f, 
			DeviceDetails.OutputFormat.Format.wBitsPerSample,
			DeviceDetails.OutputFormat.dwChannelMask );

		if( !GetOutputMatrix( DeviceDetails.OutputFormat.dwChannelMask, NumSpeakers ) )
		{
			debugf( NAME_Init, TEXT( "Unsupported speaker configuration for this number of channels" ) );
			XAudio2 = NULL;
			return( FALSE );
		}

		// Create the final output voice with either 2 or 6 channels
		if( XAudio2->CreateMasteringVoice( &MasteringVoice, NumSpeakers, SampleRate, 0, 0, NULL ) != S_OK )
		{
			debugf( NAME_Init, TEXT( "Failed to create the mastering voice for XAudio2" ) );
			XAudio2 = NULL;
			return( FALSE );
		}
	}

	return( TRUE );
}

/** 
 * Derives the output matrix to use based on the channel mask and the number of channels
 */
const FLOAT OutputMatrixMono[SPEAKER_COUNT] = 
{ 
	0.7f, 0.7f, 0.5f, 0.0f, 0.5f, 0.5f	
};

const FLOAT OutputMatrix2dot0[SPEAKER_COUNT * 2] = 
{ 
	1.0f, 0.0f, 0.7f, 0.0f, 1.25f, 0.0f, // FL 
	0.0f, 1.0f, 0.7f, 0.0f, 0.0f, 1.25f  // FR
};

//	const FLOAT OutputMatrixDownMix[SPEAKER_COUNT * 2] = 
//	{ 
//		1.0f, 0.0f, 0.7f, 0.0f, 0.87f, 0.49f,  
//		0.0f, 1.0f, 0.7f, 0.0f, 0.49f, 0.87f 
//	};

const FLOAT OutputMatrix2dot1[SPEAKER_COUNT * 3] = 
{ 
	// Same as stereo, but also passing in LFE signal
	1.0f, 0.0f, 0.7f, 0.0f, 1.25f, 0.0f, // FL
	0.0f, 1.0f, 0.7f, 0.0f, 0.0f, 1.25f, // FR
	0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f,  // LFE
};

const FLOAT OutputMatrix4dot0s[SPEAKER_COUNT * 4] = 
{ 
	// Combine both rear channels to make a rear center channel
	1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, // FL
	0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, // FR
	0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, // FC
	0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f  // RC
};

const FLOAT OutputMatrix4dot0[SPEAKER_COUNT * 4] = 
{ 
	// Split the center channel to the front two speakers
	1.0f, 0.0f, 0.7f, 0.0f, 0.0f, 0.0f, // FL
	0.0f, 1.0f, 0.7f, 0.0f, 0.0f, 0.0f, // FR
	0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, // RL
	0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f  // RR
};

const FLOAT OutputMatrix4dot1[SPEAKER_COUNT * 5] = 
{ 
	// Split the center channel to the front two speakers
	1.0f, 0.0f, 0.7f, 0.0f, 0.0f, 0.0f, // FL
	0.0f, 1.0f, 0.7f, 0.0f, 0.0f, 0.0f, // FR
	0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, // LFE
	0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, // RL
	0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f  // RR
};

const FLOAT OutputMatrix5dot0[SPEAKER_COUNT * 5] = 
{ 
	// Split the center channel to the front two speakers
	1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, // FL
	0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, // FR
	0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, // FC
	0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, // SL
	0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f  // SR
};

const FLOAT OutputMatrix5dot1[SPEAKER_COUNT * 6] = 
{ 
	// Classic 5.1 setup
	1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, // FL
	0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, // FR
	0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, // FC
	0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, // LFE
	0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, // RL
	0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f  // RR
};

const FLOAT OutputMatrix5dot1s[SPEAKER_COUNT * 6] = 
{ 
	// Classic 5.1 setup
	1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, // FL
	0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, // FR
	0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, // FC
	0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, // LFE
	0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, // SL
	0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f  // SR
};

const FLOAT OutputMatrix6dot1[SPEAKER_COUNT * 7] = 
{ 
	1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, // FL
	0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, // FR
	0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, // FC
	0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, // LFE
	0.0f, 0.0f, 0.0f, 0.0f, 0.8f, 0.0f, // RL
	0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.8f, // RR
	0.0f, 0.0f, 0.0f, 0.0f, 0.5f, 0.5f, // RC
};

const FLOAT OutputMatrix7dot1[SPEAKER_COUNT * 8] = 
{ 
	0.7f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, // FL
	0.0f, 0.7f, 0.0f, 0.0f, 0.0f, 0.0f, // FR
	0.0f, 0.0f, 0.7f, 0.0f, 0.0f, 0.0f, // FC
	0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, // LFE
	0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, // RL
	0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, // RR
	0.7f, 0.0f, 0.5f, 0.0f, 0.0f, 0.0f, // FCL
	0.0f, 0.7f, 0.5f, 0.0f, 0.0f, 0.0f  // FCR
};

const FLOAT OutputMatrix7dot1s[SPEAKER_COUNT * 8] = 
{ 
	// Split the rear channels evenly between side and rear
	1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, // FL
	0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, // FR
	0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, // FC
	0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, // LFE
	0.0f, 0.0f, 0.0f, 0.0f, 0.7f, 0.0f, // RL
	0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.7f, // RR
	0.0f, 0.0f, 0.0f, 0.0f, 0.7f, 0.0f, // SL
	0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.7f  // SR
};

typedef struct SOuputMapping
{
	DWORD NumChannels;
	DWORD SpeakerMask;
	const FLOAT* OuputMatrix;
} TOuputMapping;

TOuputMapping OutputMappings[] =
{
	{ 1, SPEAKER_MONO, OutputMatrixMono },
	{ 2, SPEAKER_STEREO, OutputMatrix2dot0 },
	{ 3, SPEAKER_2POINT1, OutputMatrix2dot1 },
	{ 4, SPEAKER_SURROUND, OutputMatrix4dot0s },
	{ 4, SPEAKER_QUAD, OutputMatrix4dot0 },
	{ 5, SPEAKER_4POINT1, OutputMatrix4dot1 },
	{ 5, SPEAKER_5POINT0, OutputMatrix5dot0 },
	{ 6, SPEAKER_5POINT1, OutputMatrix5dot1 },
	{ 6, SPEAKER_5POINT1_SURROUND, OutputMatrix5dot1s },
	{ 7, SPEAKER_6POINT1, OutputMatrix6dot1 },
	{ 8, SPEAKER_7POINT1, OutputMatrix7dot1 },
	{ 8, SPEAKER_7POINT1_SURROUND, OutputMatrix7dot1s }
};

UBOOL UXAudio2Device::GetOutputMatrix( DWORD ChannelMask, DWORD NumChannels )
{
	// Default is vanilla stereo
	OutputMixMatrix = OutputMatrix2dot0;

	// Find the best match
	for( int MappingIndex = 0; MappingIndex < sizeof( OutputMappings ) / sizeof( TOuputMapping ); MappingIndex++ )
	{
		if( NumChannels != OutputMappings[MappingIndex].NumChannels )
		{
			continue;
		}

		if( ( ChannelMask & OutputMappings[MappingIndex].SpeakerMask ) != ChannelMask )
		{
			continue;
		}

		OutputMixMatrix = OutputMappings[MappingIndex].OuputMatrix;
		break;
	}

	return( OutputMixMatrix != NULL );
}

/**
 * Update the audio device and calculates the cached inverse transform later
 * on used for spatialization.
 *
 * @param	Realtime	whether we are paused or not
 */
void UXAudio2Device::Update( UBOOL bRealtime )
{
	Super::Update( bRealtime );

	// Caches the matrix used to transform a sounds position into local space so we can just look
	// at the Y component after normalization to determine spatialization.
	InverseTransform = FMatrix( Listeners( 0 ).Up, Listeners( 0 ).Right, Listeners( 0 ).Up ^ Listeners( 0 ).Right, Listeners( 0 ).Location ).Inverse();

	// Print statistics for first non initial load allocation.
	static UBOOL bFirstTime = TRUE;
	if( bFirstTime && CommonAudioPoolSize != 0 )
	{
		bFirstTime = FALSE;
		if( CommonAudioPoolFreeBytes != 0 )
		{
			debugf( TEXT( "XAudio2: Audio pool size mismatch by %d bytes. Please update CommonAudioPoolSize ini setting to %d to avoid waste!" ),
									CommonAudioPoolFreeBytes, CommonAudioPoolSize - CommonAudioPoolFreeBytes );
		}
	}
}

/**
 * Precaches the passed in sound node wave object.
 *
 * @param	SoundNodeWave	Resource to be precached.
 */
void UXAudio2Device::Precache( USoundNodeWave* SoundNodeWave )
{
	FXAudio2SoundBuffer::GetSoundFormat( SoundNodeWave, GIsEditor ? MinCompressedDurationEditor : MinCompressedDurationGame );
#if _WINDOWS
	if( SoundNodeWave->VorbisDecompressor == NULL && SoundNodeWave->DecompressionType == DTYPE_Native )
	{
		// Grab the compressed vorbis data
		SoundNodeWave->InitAudioResource( SoundNodeWave->CompressedPCData );

		check(!SoundNodeWave->VorbisDecompressor); // should not have had a valid pointer at this point
		// Create a worker to decompress the vorbis data
		SoundNodeWave->VorbisDecompressor = new FAsyncVorbisDecompress( SoundNodeWave );
		SoundNodeWave->VorbisDecompressor->StartBackgroundTask();
	}
	else
	{
		// If it's not native, then it will remain compressed
		INC_DWORD_STAT_BY( STAT_AudioMemorySize, SoundNodeWave->GetResourceSize() );
		INC_DWORD_STAT_BY( STAT_AudioMemory, SoundNodeWave->GetResourceSize() );
	}
#elif XBOX
	FXAudio2SoundBuffer::Init( this, SoundNodeWave );
	// Size of the decompressed data
	INC_DWORD_STAT_BY( STAT_AudioMemorySize, SoundNodeWave->GetResourceSize() );
	INC_DWORD_STAT_BY( STAT_AudioMemory, SoundNodeWave->GetResourceSize() );
#endif
}

/**
 * Frees the resources associated with this buffer
 *
 * @param	FXAudio2SoundBuffer	Buffer to clean up
 */
void UXAudio2Device::FreeBufferResource( FXAudio2SoundBuffer* Buffer )
{
	if( Buffer )
	{
		// Remove from buffers array.
		Buffers.RemoveItem( Buffer );

		// See if it is being used by a sound source...
		for( INT SrcIndex = 0; SrcIndex < Sources.Num(); SrcIndex++ )
		{
			FXAudio2SoundSource* Src = ( FXAudio2SoundSource* )( Sources( SrcIndex ) );
			if( Src && Src->Buffer && ( Src->Buffer == Buffer ) )
			{
				// Make sure the buffer is no longer referenced by anything
				Src->Stop();
			}
		}

		// Delete it. This will automatically remove itself from the WaveBufferMap.
		delete Buffer;
	}
}

/**
 * Frees the bulk resource data associated with this SoundNodeWave.
 *
 * @param	SoundNodeWave	wave object to free associated bulk data
 */
void UXAudio2Device::FreeResource( USoundNodeWave* SoundNodeWave )
{
	// Find buffer for resident wavs
	if( SoundNodeWave->ResourceID )
	{
		// Find buffer associated with resource id.
		FXAudio2SoundBuffer* Buffer = WaveBufferMap.FindRef( SoundNodeWave->ResourceID );
		FreeBufferResource( Buffer );

		SoundNodeWave->ResourceID = 0;
	}

	// Just in case the data was created but never uploaded
	if( SoundNodeWave->RawPCMData )
	{
		appFree( SoundNodeWave->RawPCMData );
		SoundNodeWave->RawPCMData = NULL;
	}

	// Licensee suggested fix
	if( SoundNodeWave->VorbisDecompressor ) 
	{
		delete SoundNodeWave->VorbisDecompressor;
		SoundNodeWave->VorbisDecompressor = NULL;
	}

	// Remove the compressed copy of the data
	SoundNodeWave->RemoveAudioResource();

	// Stat housekeeping
	DEC_DWORD_STAT_BY( STAT_AudioMemorySize, SoundNodeWave->GetResourceSize() );
	DEC_DWORD_STAT_BY( STAT_AudioMemory, SoundNodeWave->GetResourceSize() );
}

// sort memory usage from large to small unless bAlphaSort
static UBOOL bAlphaSort = FALSE;
IMPLEMENT_COMPARE_POINTER( FXAudio2SoundBuffer, XeAudioDevice, { return bAlphaSort ? appStricmp( *A->ResourceName,*B->ResourceName ) : ( A->GetSize() > B->GetSize() ) ? -1 : 1; } );


/**
 * This will return the name of the SoundClass of the SoundCue that this buffer(soundnodewave) belongs to.
 * NOTE: This will find the first cue in the ObjectIterator list.  So if we are using SoundNodeWaves in multiple
 * SoundCues we will pick up the first first one only.
 **/
static FName GetSoundClassNameFromBuffer( const FXAudio2SoundBuffer* const Buffer )
{
	// for each buffer
	// look at all of the SoundCue's SoundNodeWaves to see if the ResourceID matched
	// if it does then grab the SoundClass of the SoundCue (that the waves werre gotten from)

	for( TObjectIterator<USoundCue> It; It; ++It )
	{
		USoundCue* Cue = *It;
		TArray<USoundNodeWave*> OutWaves;
		Cue->RecursiveFindNode<USoundNodeWave>( Cue->FirstNode, OutWaves );

		for( INT WaveIndex = 0; WaveIndex < OutWaves.Num(); WaveIndex++ )
		{
			USoundNodeWave* WaveNode = OutWaves(WaveIndex);
			if( WaveNode != NULL )
			{
				if( WaveNode->ResourceID == Buffer->ResourceID )
				{
					return Cue->SoundClass;
				}
			}
		}
	}

	return NAME_None;
}


/** 
 * Displays debug information about the loaded sounds
 */
void UXAudio2Device::ListSounds( const TCHAR* Cmd, FOutputDevice& Ar )
{
	bAlphaSort = ParseParam( Cmd, TEXT( "ALPHASORT" ) );

	INT	TotalResident = 0;
	INT	ResidentCount = 0;

	Ar.Logf( TEXT("Listing all sounds.") );
	Ar.Logf( TEXT( ", Size Kb, NumChannels, SoundName, bAllocationInPermanentPool, SoundClass" ) );

	TArray<FXAudio2SoundBuffer*> AllSounds;
	for( INT BufferIndex = 0; BufferIndex < Buffers.Num(); BufferIndex++ )
	{
		AllSounds.AddItem( Buffers( BufferIndex ) );
	}

	Sort<USE_COMPARE_POINTER( FXAudio2SoundBuffer, XeAudioDevice )>( &AllSounds( 0 ), AllSounds.Num() );

	for( INT i = 0; i < AllSounds.Num(); ++i )
	{
		FXAudio2SoundBuffer* Buffer = AllSounds( i );

		const FName SoundClassName = GetSoundClassNameFromBuffer( Buffer );

		Ar.Logf( TEXT( ", %8.2f, %d channel(s), %s, %d, %s" ), Buffer->GetSize() / 1024.0f, Buffer->NumChannels, *Buffer->ResourceName, Buffer->bAllocationInPermanentPool, *SoundClassName.ToString() );



		TotalResident += Buffer->GetSize();
		ResidentCount++;
	}

	Ar.Logf( TEXT( "%8.2f Kb for %d resident sounds" ), TotalResident / 1024.0f, ResidentCount );
}

/** Test decompress a vorbis file */
void UXAudio2Device::TestDecompressOggVorbis( USoundNodeWave* Wave )
{
	FVorbisAudioInfo	OggInfo;
	FSoundQualityInfo	QualityInfo = { 0 };

	// Parse the ogg vorbis header for the relevant information
	if( OggInfo.ReadCompressedInfo( Wave->ResourceData, Wave->ResourceSize, &QualityInfo ) )
	{
		// Extract the data
		Wave->SampleRate = QualityInfo.SampleRate;
		Wave->NumChannels = QualityInfo.NumChannels;
		Wave->RawPCMDataSize = QualityInfo.SampleDataSize;
		Wave->Duration = QualityInfo.Duration;

		Wave->RawPCMData = ( BYTE* )appMalloc( Wave->RawPCMDataSize );

		// Decompress all the sample data (and preallocate memory)
		OggInfo.ExpandFile( Wave->RawPCMData, &QualityInfo );

		appFree( Wave->RawPCMData );
	}
}

/** Decompress a wav a number of times for profiling purposes */
void UXAudio2Device::TimeTest( FOutputDevice& Ar, const TCHAR* WaveAssetName )
{
	USoundNodeWave* Wave = LoadObject<USoundNodeWave>( NULL, WaveAssetName, NULL, LOAD_NoWarn, NULL );
	if( Wave )
	{
		// Wait for initial decompress
		if( Wave->VorbisDecompressor )
		{
			while( !Wave->VorbisDecompressor->IsDone() )
			{
			}

			delete Wave->VorbisDecompressor;
			Wave->VorbisDecompressor = NULL;
		}
		
		// If the wave loaded in fine, time the decompression
		Wave->InitAudioResource( Wave->CompressedPCData );

		DOUBLE Start = appSeconds();

		for( INT i = 0; i < 1000; i++ )
		{
			TestDecompressOggVorbis( Wave );
		} 

		DOUBLE Duration = appSeconds() - Start;
		Ar.Logf( TEXT( "%s: %g ms - %g ms per second per channel" ), WaveAssetName, Duration, Duration / ( Wave->Duration * Wave->NumChannels ) );

		Wave->RemoveAudioResource();
	}
	else
	{
		Ar.Logf( TEXT( "Failed to find test file '%s' to decompress" ), WaveAssetName );
	}
}


/**
 * Exec handler used to parse console commands.
 *
 * @param	Cmd		Command to parse
 * @param	Ar		Output device to use in case the handler prints anything
 * @return	TRUE if command was handled, FALSE otherwise
 */
UBOOL UXAudio2Device::Exec( const TCHAR* Cmd, FOutputDevice& Ar )
{
	if( Super::Exec( Cmd, Ar ) )
	{
		return( TRUE );
	}
	else if( ParseCommand( &Cmd, TEXT( "TestVorbisDecompressionSpeed" ) ) )
	{

		TimeTest( Ar, TEXT( "TestSounds.44Mono_TestWeaponSynthetic" ) );
		TimeTest( Ar, TEXT( "TestSounds.44Mono_TestDialogFemale" ) );
		TimeTest( Ar, TEXT( "TestSounds.44Mono_TestDialogMale" ) );

		TimeTest( Ar, TEXT( "TestSounds.22Mono_TestWeaponSynthetic" ) );
		TimeTest( Ar, TEXT( "TestSounds.22Mono_TestDialogFemale" ) );
		TimeTest( Ar, TEXT( "TestSounds.22Mono_TestDialogMale" ) );

		TimeTest( Ar, TEXT( "TestSounds.22Stereo_TestMusicAcoustic" ) );
		TimeTest( Ar, TEXT( "TestSounds.44Stereo_TestMusicAcoustic" ) );
	}

	return( FALSE );
}

/**
 * Allocates memory from permanent pool. This memory will NEVER be freed.
 *
 * @param	Size	Size of allocation.
 *
 * @return pointer to a chunk of memory with size Size
 */
void* UXAudio2Device::AllocatePermanentMemory( INT Size, UBOOL& AllocatedInPool )
{
	void* Allocation = NULL;
	
	// Fall back to using regular allocator if there is not enough space in permanent memory pool.
	if( Size > CommonAudioPoolFreeBytes )
	{
		Allocation = appPhysicalAlloc( Size, CACHE_Normal );
		check( Allocation );

		AllocatedInPool = FALSE;
	}
	// Allocate memory from pool.
	else
	{
		BYTE* CommonAudioPoolAddress = ( BYTE* )CommonAudioPool;
		Allocation = CommonAudioPoolAddress + ( CommonAudioPoolSize - CommonAudioPoolFreeBytes );

		AllocatedInPool = TRUE;
	}

	// Decrement available size regardless of whether we allocated from pool or used regular allocator
	// to allow us to log suggested size at the end of initial loading.
	CommonAudioPoolFreeBytes -= Size;
	
	return( Allocation );
}

/** 
 * Links up the resource data indices for looking up and cleaning up
 */
void UXAudio2Device::TrackResource( USoundNodeWave* Wave, FXAudio2SoundBuffer* Buffer )
{
	// Allocate new resource ID and assign to USoundNodeWave. A value of 0 (default) means not yet registered.
	INT ResourceID = NextResourceID++;
	Buffer->ResourceID = ResourceID;
	Wave->ResourceID = ResourceID;

	Buffers.AddItem( Buffer );
	WaveBufferMap.Set( ResourceID, Buffer );
}

/**
 * Get information about the memory allocated for a sound.
 * @param	ResourceID	The resource to get information about.
 * @param	BytesUsed	The number of bytes used for the resource.
 * @param	IsPartOfPool	Whether the bytes came from the common audio pool or an individual allocation.
 * @return	True if successful, false if ResourceID wasn't found or some other error occured.
 */
UBOOL UXAudio2Device::GetResourceAllocationInfo(INT ResourceID, /*OUT*/ INT& BytesUsed, /*OUT*/ UBOOL& IsPartOfPool)
{
	FXAudio2SoundBuffer* Buffer = WaveBufferMap.FindRef( ResourceID );
	if (Buffer != NULL)
	{
		IsPartOfPool = Buffer->bAllocationInPermanentPool;
		BytesUsed = Buffer->GetSize();
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}

/*------------------------------------------------------------------------------------
	Static linking helpers.
------------------------------------------------------------------------------------*/

/**
 * Initialize registrants, basically calling StaticClass() to create the class and also 
 * populating the lookup table.
 *
 * @param	Lookup	current index into lookup table
 */
void AutoInitializeRegistrantsXAudio2( INT& Lookup )
{
	UXAudio2Device::StaticClass();
}

/**
 * Auto generates names.
 */
void AutoRegisterNamesXAudio2( void )
{
}

// end

