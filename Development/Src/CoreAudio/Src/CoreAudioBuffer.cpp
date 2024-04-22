/*=============================================================================
 	CoreAudioBuffer.cpp: Unreal CoreAudio buffer interface object.
 	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 =============================================================================*/

/*------------------------------------------------------------------------------------
 Audio includes.
 ------------------------------------------------------------------------------------*/

#include "Engine.h"
#include "EngineSoundClasses.h"
#include "EngineAudioDeviceClasses.h"
#include "UnConsoleTools.h"
#include "UnAudioDecompress.h"
#include "UnAudioEffect.h"
#include "CoreAudioDevice.h"

/*------------------------------------------------------------------------------------
 FCoreAudioSoundBuffer.
 ------------------------------------------------------------------------------------*/

/** 
 * Constructor
 *
 * @param AudioDevice	audio device this sound buffer is going to be attached to.
 */
FCoreAudioSoundBuffer::FCoreAudioSoundBuffer( UAudioDevice* InAudioDevice, ESoundFormat InSoundFormat )
:	AudioDevice( InAudioDevice ),
	SoundFormat( InSoundFormat ),
	PCMData( NULL ),
	PCMDataSize( 0 ),
	DecompressionState( NULL ),
	NumChannels( 0 ),
	ResourceID( 0 ),
	bAllocationInPermanentPool( FALSE ),
	bDynamicResource( FALSE )
{
}

/**
 * Destructor 
 * 
 * Frees wave data and detaches itself from audio device.
 */
FCoreAudioSoundBuffer::~FCoreAudioSoundBuffer( void )
{
	if( bAllocationInPermanentPool )
	{
		appErrorf( TEXT( "Can't free resource '%s' as it was allocated in permanent pool." ), *ResourceName );
	}

	if( DecompressionState )
	{
		delete DecompressionState;
	}

	if( ResourceID )
	{
		UCoreAudioDevice* CoreAudioDevice = ( UCoreAudioDevice* )AudioDevice;
		CoreAudioDevice->WaveBufferMap.Remove( ResourceID );

		if( PCMData )
		{
			appFree( PCMData );
		}
	}
}

/**
 * Returns the size of this buffer in bytes.
 *
 * @return Size in bytes
 */
INT FCoreAudioSoundBuffer::GetSize( void )
{
	INT TotalSize = 0;
	
	switch( SoundFormat )
	{
		case SoundFormat_PCM:
			TotalSize = PCMDataSize;
			break;
			
		case SoundFormat_PCMPreview:
			TotalSize = PCMDataSize;
			break;
			
		case SoundFormat_PCMRT:
			TotalSize = (DecompressionState ? DecompressionState->SrcBufferDataSize : 0) + ( MONO_PCM_BUFFER_SIZE * 2 * NumChannels );
			break;
	}
	
	return( TotalSize );
}

/** 
 * Setup an AudioStreamBasicDescription structure
 */
void FCoreAudioSoundBuffer::InitAudioStreamBasicDescription( UInt32 FormatID, USoundNodeWave* Wave, UBOOL bCheckPCMData )
{
	PCMFormat.mSampleRate = Wave->SampleRate;
	PCMFormat.mFormatID = FormatID;
	PCMFormat.mFormatID = kAudioFormatLinearPCM;
	PCMFormat.mFormatFlags = kLinearPCMFormatFlagIsPacked | kAudioFormatFlagsNativeEndian | kLinearPCMFormatFlagIsSignedInteger;
	PCMFormat.mFramesPerPacket = 1;
	PCMFormat.mChannelsPerFrame = Wave->NumChannels;
	PCMFormat.mBitsPerChannel = 16;
	PCMFormat.mBytesPerFrame = PCMFormat.mChannelsPerFrame * PCMFormat.mBitsPerChannel / 8;
	PCMFormat.mBytesPerPacket = PCMFormat.mBytesPerFrame * PCMFormat.mFramesPerPacket;
	
	// Set the number of channels - 0 channels means there has been an error
	NumChannels = Wave->NumChannels;

	if( bCheckPCMData )
	{
		if( PCMData == NULL || PCMDataSize == 0 )
		{
			NumChannels = 0;
			warnf( TEXT( "Failed to create audio buffer for '%s'" ), *Wave->GetFullName() );
		}
	}
}

/** 
 * Gets the type of buffer that will be created for this wave and stores it.
 */
void FCoreAudioSoundBuffer::GetSoundFormat( USoundNodeWave* Wave, FLOAT MinOggVorbisDuration )
{
	if( Wave == NULL )
	{
		return;
	}
	
	if( Wave->NumChannels == 0 )
	{
		// No channels - no way of knowing what to play back
		Wave->DecompressionType = DTYPE_Invalid;
	}
	else if( Wave->bUseTTS || Wave->RawPCMData )
	{
		// Run time created audio; e.g. TTS or editor preview data
		Wave->DecompressionType = DTYPE_Preview;
	}
	else if ( Wave->bProcedural )
	{
		// Streaming data, created programmatically.
		Wave->DecompressionType = DTYPE_Procedural;
	}
	else if( Wave->bForceRealTimeDecompression || ( Wave->Duration > ( MinOggVorbisDuration * Wave->NumChannels ) ) )
	{
		// Store as compressed data and decompress in realtime
		Wave->DecompressionType = DTYPE_RealTime;
	}
	else
	{
		// Fully expand loaded vorbis data into PCM
		Wave->DecompressionType = DTYPE_Native;
	}
}

/**
 * Decompresses a chunk of compressed audio to the destination memory
 *
 * @param Destination		Memory to decompress to
 * @param bLooping			Whether to loop the sound seamlessly, or pad with zeroes
 * @return					Whether the sound looped or not
 */
UBOOL FCoreAudioSoundBuffer::ReadCompressedData( BYTE* Destination, UBOOL bLooping )
{
	return( DecompressionState->ReadCompressedData( Destination, bLooping, MONO_PCM_BUFFER_SIZE * NumChannels ) );
}

/**
 * Static function used to create a CoreAudio buffer and dynamically upload decompressed ogg vorbis data to.
 *
 * @param InWave		USoundNodeWave to use as template and wave source
 * @param AudioDevice	audio device to attach created buffer to
 * @return FALSoundBuffer pointer if buffer creation succeeded, NULL otherwise
 */
FCoreAudioSoundBuffer* FCoreAudioSoundBuffer::CreateQueuedBuffer( UCoreAudioDevice* CoreAudioDevice, USoundNodeWave* Wave )
{
	// Always create a new buffer for real time decompressed sounds
	FCoreAudioSoundBuffer* Buffer = new FCoreAudioSoundBuffer( CoreAudioDevice, SoundFormat_PCMRT );
	
	// Prime the first two buffers and prepare the decompression
	FSoundQualityInfo QualityInfo = { 0 };
	
	Buffer->DecompressionState = new FVorbisAudioInfo();
	
	Wave->InitAudioResource( Wave->CompressedPCData );
	
	if( Buffer->DecompressionState->ReadCompressedInfo( Wave->ResourceData, Wave->ResourceSize, &QualityInfo ) )
	{
		// Refresh the wave data
		Wave->SampleRate = QualityInfo.SampleRate;
		Wave->NumChannels = QualityInfo.NumChannels;
		Wave->RawPCMDataSize = QualityInfo.SampleDataSize;
		Wave->Duration = QualityInfo.Duration;
		
		// Clear out any dangling pointers
		Buffer->PCMData = NULL;
		Buffer->PCMDataSize = 0;
		
		Buffer->InitAudioStreamBasicDescription( kAudioFormatLinearPCM, Wave, FALSE );
	}
	else
	{
		Wave->DecompressionType = DTYPE_Invalid;
		Wave->NumChannels = 0;
		
		Wave->RemoveAudioResource();
	}
	
	// No tracking of this resource as it's temporary
	Buffer->ResourceID = 0;
	Wave->ResourceID = 0;
	
	return( Buffer );
}

/**
 * Static function used to create an OpenAL buffer and dynamically upload procedural data to.
 *
 * @param InWave		USoundNodeWave to use as template and wave source
 * @param AudioDevice	audio device to attach created buffer to
 * @return FALSoundBuffer pointer if buffer creation succeeded, NULL otherwise
 */
FCoreAudioSoundBuffer* FCoreAudioSoundBuffer::CreateProceduralBuffer( UCoreAudioDevice* CoreAudioDevice, USoundNodeWave* Wave )
{
	// Always create a new buffer for real time decompressed sounds
	FCoreAudioSoundBuffer* Buffer = new FCoreAudioSoundBuffer( CoreAudioDevice, SoundFormat_PCMRT );
	
	// Clear out any dangling pointers
	Buffer->DecompressionState = NULL;
	Buffer->PCMData = NULL;
	Buffer->PCMDataSize = 0;
	Buffer->InitAudioStreamBasicDescription( kAudioFormatLinearPCM, Wave, FALSE );
	
	// No tracking of this resource as it's temporary
	Buffer->ResourceID = 0;
	Wave->ResourceID = 0;
	
	return( Buffer );
}

/**
 * Static function used to create an OpenAL buffer and upload raw PCM data to.
 *
 * @param InWave		USoundNodeWave to use as template and wave source
 * @param AudioDevice	audio device to attach created buffer to
 * @return FALSoundBuffer pointer if buffer creation succeeded, NULL otherwise
 */
FCoreAudioSoundBuffer* FCoreAudioSoundBuffer::CreatePreviewBuffer( UCoreAudioDevice* CoreAudioDevice, USoundNodeWave* Wave, FCoreAudioSoundBuffer* Buffer )
{
	if( Buffer )
	{
		CoreAudioDevice->FreeBufferResource( Buffer );
	}
	
	// Create new buffer.
	Buffer = new FCoreAudioSoundBuffer( CoreAudioDevice, SoundFormat_PCMPreview );
	
	// Take ownership the PCM data
	Buffer->PCMData = Wave->RawPCMData;
	Buffer->PCMDataSize = Wave->RawPCMDataSize;
	
	Wave->RawPCMData = NULL;
	
	// Copy over whether this data should be freed on delete
	Buffer->bDynamicResource = Wave->bDynamicResource;
	
	Buffer->InitAudioStreamBasicDescription( kAudioFormatLinearPCM, Wave, TRUE );
	
	CoreAudioDevice->TrackResource( Wave, Buffer );
	
	return( Buffer );
}

/**
 * Static function used to create a CoreAudio buffer and upload decompressed ogg vorbis data to.
 *
 * @param Wave			USoundNodeWave to use as template and wave source
 * @param AudioDevice	audio device to attach created buffer to
 * @return FCoreAudioSoundBuffer pointer if buffer creation succeeded, NULL otherwise
 */
FCoreAudioSoundBuffer* FCoreAudioSoundBuffer::CreateNativeBuffer( UCoreAudioDevice* CoreAudioDevice, USoundNodeWave* Wave )
{
	// Check to see if thread has finished decompressing on the other thread
	if( Wave->VorbisDecompressor != NULL )
	{
		if( !Wave->VorbisDecompressor->IsDone() )
		{
			// Don't play this sound just yet
			debugf( NAME_DevAudio, TEXT( "Waiting for sound to decompress: %s" ), *Wave->GetName() );
			return( NULL );
		}
		
		// Remove the decompressor
		delete Wave->VorbisDecompressor;
		Wave->VorbisDecompressor = NULL;
	}
	
	// Create new buffer.
	FCoreAudioSoundBuffer* Buffer = new FCoreAudioSoundBuffer( CoreAudioDevice, SoundFormat_PCM );

	// Take ownership the PCM data
	Buffer->PCMData = Wave->RawPCMData;
	Buffer->PCMDataSize = Wave->RawPCMDataSize;
	
	Wave->RawPCMData = NULL;

	// Keep track of associated resource name.
	Buffer->InitAudioStreamBasicDescription( kAudioFormatLinearPCM, Wave, TRUE );
	
	CoreAudioDevice->TrackResource( Wave, Buffer );
	
	Wave->RemoveAudioResource();
	
	return( Buffer );
}

/**
 * Static function used to create a buffer.
 *
 * @param InWave USoundNodeWave to use as template and wave source
 * @param AudioDevice audio device to attach created buffer to
 * @return FCoreAudioSoundBuffer pointer if buffer creation succeeded, NULL otherwise
 */
FCoreAudioSoundBuffer* FCoreAudioSoundBuffer::Init( UAudioDevice* AudioDevice, USoundNodeWave* Wave )
{
	// Can't create a buffer without any source data
	if( Wave == NULL || Wave->NumChannels == 0 )
	{
		return NULL;
	}

	UCoreAudioDevice *CoreAudioDevice = ( UCoreAudioDevice *)AudioDevice;
	FCoreAudioSoundBuffer *Buffer = NULL;

	switch( Wave->DecompressionType )
	{
		case DTYPE_Setup:
			// Has circumvented precache mechanism - precache now
			GetSoundFormat( Wave, CoreAudioDevice->MinCompressedDurationGame );
			
			if( Wave->DecompressionType == DTYPE_Native )
			{
				// Grab the compressed vorbis data
				Wave->InitAudioResource( Wave->CompressedPCData );
				
				// should not have had a valid pointer at this point
				check( !Wave->VorbisDecompressor ); 
				
				// Create a worker to decompress the vorbis data
				FAsyncVorbisDecompress TempDecompress( Wave );
				TempDecompress.StartSynchronousTask();
			}
			
			// Recall this function with new decompression type
			return( Init( AudioDevice, Wave ) );
			
		case DTYPE_Preview:
			// Find the existing buffer if any
			if( Wave->ResourceID )
			{
				Buffer = CoreAudioDevice->WaveBufferMap.FindRef( Wave->ResourceID );
			}
			
			// Override with any new PCM data even if some already exists. 
			if( Wave->RawPCMData )
			{
				// Upload the preview PCM data to it
				Buffer = CreatePreviewBuffer( CoreAudioDevice, Wave, Buffer );
			}
			break;
			
		case DTYPE_Procedural:
			// Always create a new buffer for streaming procedural data
			Buffer = CreateProceduralBuffer( CoreAudioDevice, Wave );
			break;
			
		case DTYPE_RealTime:
			// Always create a new buffer for streaming ogg vorbis data
			Buffer = CreateQueuedBuffer( CoreAudioDevice, Wave );
			break;
			
		case DTYPE_Native:
			// Upload entire wav to CoreAudio
			if( Wave->ResourceID )
			{
				Buffer = CoreAudioDevice->WaveBufferMap.FindRef( Wave->ResourceID );
			}

			if( Buffer == NULL )
			{
				Buffer = CreateNativeBuffer( CoreAudioDevice, Wave );
			}
			break;
			
		case DTYPE_Invalid:
		default:
			// Invalid will be set if the wave cannot be played
			break;
	}

#if !FINAL_RELEASE
	if( Buffer )
	{
		// Keep track of associated resource name.
		Buffer->ResourceName = Wave->GetPathName();
	}
#endif

	return Buffer;
}
