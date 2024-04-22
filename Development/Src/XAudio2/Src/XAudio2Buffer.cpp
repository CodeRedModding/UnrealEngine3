/*=============================================================================
	XeAudioDevice.cpp: Unreal XAudio2 Audio interface object.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

	Unreal is RHS with Y and Z swapped (or technically LHS with flipped axis)

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
#include "XAudio2Device.h"
#include "XAudio2Effects.h"

/**
 * Helper structure to access information in raw XMA data.
 */
struct FXMAInfo
{
	/**
	 * Constructor, parsing passed in raw data.
	 *
	 * @param RawData		raw XMA data
	 * @param RawDataSize	size of raw data in bytes
	 */
	FXMAInfo( BYTE* RawData, UINT RawDataSize )
	{
		// Check out XeTools.cpp/dll.
		UINT Offset = 0;
		appMemcpy( &EncodedBufferFormatSize, RawData + Offset, sizeof( DWORD ) );
		Offset += sizeof( DWORD );
		appMemcpy( &SeekTableSize, RawData + Offset, sizeof( DWORD ) );
		Offset += sizeof( DWORD );
		appMemcpy( &EncodedBufferSize, RawData + Offset, sizeof( DWORD ) );
		Offset += sizeof( DWORD );

		//@warning EncodedBufferFormat is NOT endian swapped.

		EncodedBufferFormat = ( XMA2WAVEFORMATEX* )( RawData + Offset );
		Offset += EncodedBufferFormatSize;
		SeekTable = ( DWORD* )( RawData + Offset );
		Offset += SeekTableSize;
		EncodedBuffer = RawData + Offset;
		Offset += EncodedBufferSize;

		check( Offset == RawDataSize );
	}

	/** Encoded buffer data (allocated via malloc from within XMA encoder) */
	void*				EncodedBuffer;
	/** Size in bytes of encoded buffer */
	DWORD				EncodedBufferSize;
	/** Encoded buffer format (allocated via malloc from within XMA encoder) */
	void*				EncodedBufferFormat;
	/** Size in bytes of encoded buffer format */
	DWORD				EncodedBufferFormatSize;
	/** Seek table (allocated via malloc from within XMA encoder) */
	DWORD*				SeekTable;
	/** Size in bytes of seek table */
	DWORD				SeekTableSize;
};

/*------------------------------------------------------------------------------------
	FXAudio2SoundBuffer.
------------------------------------------------------------------------------------*/

/** 
 * Constructor
 *
 * @param AudioDevice	audio device this sound buffer is going to be attached to.
 */
FXAudio2SoundBuffer::FXAudio2SoundBuffer( UAudioDevice* InAudioDevice, ESoundFormat InSoundFormat )
:	AudioDevice( InAudioDevice ),
	SoundFormat( InSoundFormat ),
	DecompressionState( NULL ),
	NumChannels( 0 ),
	ResourceID( 0 ),
	bAllocationInPermanentPool( FALSE ),
	bDynamicResource( FALSE )
{
	PCM.PCMData = NULL;
	PCM.PCMDataSize = 0;

	XMA2.XMA2Data = NULL;
	XMA2.XMA2DataSize = 0;

	XWMA.XWMAData = NULL;
	XWMA.XWMADataSize = 0;
	XWMA.XWMASeekData = NULL;
	XWMA.XWMASeekDataSize = 0;
}

/**
 * Destructor 
 * 
 * Frees wave data and detaches itself from audio device.
 */
FXAudio2SoundBuffer::~FXAudio2SoundBuffer( void )
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
		UXAudio2Device* XAudio2Device = ( UXAudio2Device* )AudioDevice;
		XAudio2Device->WaveBufferMap.Remove( ResourceID );

		switch( SoundFormat )
		{
		case SoundFormat_PCM:
			if( PCM.PCMData )
			{
				appFree( ( void* )PCM.PCMData );
			}
			break;

		case SoundFormat_PCMPreview:
			if( bDynamicResource && PCM.PCMData )
			{
				appFree( ( void* )PCM.PCMData );
			}
			break;

		case SoundFormat_PCMRT:
			// Buffers are freed as part of the ~FSoundSource
			break;

		case SoundFormat_XMA2:
			if( XMA2.XMA2Data )
			{
				// Wave data was kept in pBuffer so we need to free it.
				appPhysicalFree( ( void* )XMA2.XMA2Data );
			}
			break;

		case SoundFormat_XWMA:
			if( XWMA.XWMAData )
			{
				// Wave data was kept in pBuffer so we need to free it.
				appFree( ( void* )XWMA.XWMAData );
			}

			if( XWMA.XWMASeekData )
			{
				// Wave data was kept in pBuffer so we need to free it.
				appFree( ( void* )XWMA.XWMASeekData );
			}
			break;
		}
	}
}

/**
 * Returns the size of this buffer in bytes.
 *
 * @return Size in bytes
 */
INT FXAudio2SoundBuffer::GetSize( void )
{
	INT TotalSize = 0;

	switch( SoundFormat )
	{
	case SoundFormat_PCM:
		TotalSize = PCM.PCMDataSize;
		break;

	case SoundFormat_PCMPreview:
		TotalSize = PCM.PCMDataSize;
		break;

	case SoundFormat_PCMRT:
		TotalSize = (DecompressionState ? DecompressionState->SrcBufferDataSize : 0) + ( MONO_PCM_BUFFER_SIZE * 2 * NumChannels );
		break;

	case SoundFormat_XMA2:
		TotalSize = XMA2.XMA2DataSize;
		break;

	case SoundFormat_XWMA:
		TotalSize = XWMA.XWMADataSize + XWMA.XWMASeekDataSize;
		break;
	}

	return( TotalSize );
}

/** 
 * Setup a WAVEFORMATEX structure
 */
void FXAudio2SoundBuffer::InitWaveFormatEx( WORD Format, USoundNodeWave* Wave, UBOOL bCheckPCMData )
{
	// Setup the format structure required for XAudio2
	PCM.PCMFormat.wFormatTag = Format;
	PCM.PCMFormat.nChannels = Wave->NumChannels;
	PCM.PCMFormat.nSamplesPerSec = Wave->SampleRate;
	PCM.PCMFormat.wBitsPerSample = 16;
	PCM.PCMFormat.cbSize = 0;

	// Set the number of channels - 0 channels means there has been an error
	NumChannels = Wave->NumChannels;

	if( bCheckPCMData )
	{
		if( PCM.PCMData == NULL || PCM.PCMDataSize == 0 )
		{
			NumChannels = 0;
			warnf( TEXT( "Failed to create audio buffer for '%s'" ), *Wave->GetFullName() );
		}
	}

	PCM.PCMFormat.nBlockAlign = NumChannels * sizeof( SWORD );
	PCM.PCMFormat.nAvgBytesPerSec = NumChannels * sizeof( SWORD ) * Wave->SampleRate;
}

/** 
 * Set up this buffer to contain and play XMA2 data
 */
void FXAudio2SoundBuffer::InitXMA2( UXAudio2Device* XAudio2Device, USoundNodeWave* Wave, FXMAInfo* XMAInfo )
{
	SoundFormat = SoundFormat_XMA2;

	appMemcpy( &XMA2, XMAInfo->EncodedBufferFormat, XMAInfo->EncodedBufferFormatSize );

	NumChannels = XMA2.XMA2Format.wfx.nChannels;

	// Allocate the audio data in physical memory
	XMA2.XMA2DataSize = XMAInfo->EncodedBufferSize;

	if( Wave->HasAnyFlags( RF_RootSet ) )
	{
		// Allocate from permanent pool and mark buffer as non destructible.
		UBOOL AllocatedInPool = TRUE;
		XMA2.XMA2Data = ( BYTE* )XAudio2Device->AllocatePermanentMemory( XMA2.XMA2DataSize, /*OUT*/ AllocatedInPool );
		bAllocationInPermanentPool = AllocatedInPool;
	}
	else
	{
		// Allocate via normal allocator.
		XMA2.XMA2Data = ( BYTE* )appPhysicalAlloc( XMA2.XMA2DataSize, CACHE_Normal );
	}

	appMemcpy( ( void* )XMA2.XMA2Data, XMAInfo->EncodedBuffer, XMAInfo->EncodedBufferSize );
}

/** 
 * Set up this buffer to contain and play XWMA data
 */
void FXAudio2SoundBuffer::InitXWMA( USoundNodeWave* Wave, FXMAInfo* XMAInfo )
{
	SoundFormat = SoundFormat_XWMA;

	appMemcpy( &XWMA.XWMAFormat, XMAInfo->EncodedBufferFormat, XMAInfo->EncodedBufferFormatSize );

	NumChannels = XWMA.XWMAFormat.Format.nChannels;

	// Allocate the audio data in physical memory
	XWMA.XWMADataSize = XMAInfo->EncodedBufferSize;

	// Allocate via normal allocator.
	XWMA.XWMAData = ( BYTE* )appMalloc( XWMA.XWMADataSize );
	appMemcpy( ( void* )XWMA.XWMAData, XMAInfo->EncodedBuffer, XMAInfo->EncodedBufferSize );

	XWMA.XWMASeekDataSize = XMAInfo->SeekTableSize;

	XWMA.XWMASeekData = ( UINT32* )appMalloc( XWMA.XWMASeekDataSize );
	appMemcpy( ( void* )XWMA.XWMASeekData, XMAInfo->SeekTable, XMAInfo->SeekTableSize );
}

/** 
 * Handle Xenon specific bulk data
 */
FXAudio2SoundBuffer* FXAudio2SoundBuffer::InitXenon( UXAudio2Device* XAudio2Device, USoundNodeWave* Wave )
{
	FXAudio2SoundBuffer* Buffer = NULL;

	// Check whether there is any raw data
	INT RawDataSize = Wave->CompressedXbox360Data.GetBulkDataSize();
	if( RawDataSize > 0 )
	{
		// Create new buffer.
		Buffer = new FXAudio2SoundBuffer( XAudio2Device, SoundFormat_XMA2 );

		// Load raw data.
		void* RawData = Wave->CompressedXbox360Data.Lock( LOCK_READ_ONLY );

		// Create XMA helper to parse raw data.
		FXMAInfo XMAInfo( ( BYTE* )RawData, RawDataSize );

		if( XMAInfo.EncodedBufferFormatSize == sizeof( XMA2WAVEFORMATEX ) )
		{
			Buffer->InitXMA2( XAudio2Device, Wave, &XMAInfo );
		}
		else if( XMAInfo.EncodedBufferFormatSize == sizeof( WAVEFORMATEXTENSIBLE ) )
		{
			Buffer->InitXWMA( Wave, &XMAInfo );
		}
		else
		{
			appErrorf( TEXT( "Invalid encoded buffer format size for '%s'." ), *Wave->GetFullName() );
		}

		// Unload raw data.
		Wave->CompressedXbox360Data.Unlock();

		XAudio2Device->TrackResource( Wave, Buffer );
	}

	return( Buffer );
}

/**
 * Decompresses a chunk of compressed audio to the destination memory
 *
 * @param Destination		Memory to decompress to
 * @param bLooping			Whether to loop the sound seamlessly, or pad with zeroes
 * @return					Whether the sound looped or not
 */
UBOOL FXAudio2SoundBuffer::ReadCompressedData( BYTE* Destination, UBOOL bLooping )
{
	return( DecompressionState->ReadCompressedData( Destination, bLooping, MONO_PCM_BUFFER_SIZE * NumChannels ) );
}

/**
 * Static function used to create an OpenAL buffer and dynamically upload decompressed ogg vorbis data to.
 *
 * @param InWave		USoundNodeWave to use as template and wave source
 * @param AudioDevice	audio device to attach created buffer to
 * @return FALSoundBuffer pointer if buffer creation succeeded, NULL otherwise
 */
FXAudio2SoundBuffer* FXAudio2SoundBuffer::CreateQueuedBuffer( UXAudio2Device* XAudio2Device, USoundNodeWave* Wave )
{
	// Always create a new buffer for real time decompressed sounds
	FXAudio2SoundBuffer* Buffer = new FXAudio2SoundBuffer( XAudio2Device, SoundFormat_PCMRT );

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
		Buffer->PCM.PCMData = NULL;
		Buffer->PCM.PCMDataSize = 0;

		Buffer->InitWaveFormatEx( WAVE_FORMAT_PCM, Wave, FALSE );
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
FXAudio2SoundBuffer* FXAudio2SoundBuffer::CreateProceduralBuffer( UXAudio2Device* XAudio2Device, USoundNodeWave* Wave )
{
	// Always create a new buffer for real time decompressed sounds
	FXAudio2SoundBuffer* Buffer = new FXAudio2SoundBuffer( XAudio2Device, SoundFormat_PCMRT );

	// Clear out any dangling pointers
	Buffer->DecompressionState = NULL;
	Buffer->PCM.PCMData = NULL;
	Buffer->PCM.PCMDataSize = 0;
	Buffer->InitWaveFormatEx( WAVE_FORMAT_PCM, Wave, FALSE );

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
FXAudio2SoundBuffer* FXAudio2SoundBuffer::CreatePreviewBuffer( UXAudio2Device* XAudio2Device, USoundNodeWave* Wave, FXAudio2SoundBuffer* Buffer )
{
	if( Buffer )
	{
		XAudio2Device->FreeBufferResource( Buffer );
	}

	// Create new buffer.
	Buffer = new FXAudio2SoundBuffer( XAudio2Device, SoundFormat_PCMPreview );

	// Take ownership the PCM data
	Buffer->PCM.PCMData = Wave->RawPCMData;
	Buffer->PCM.PCMDataSize = Wave->RawPCMDataSize;

	Wave->RawPCMData = NULL;

	// Copy over whether this data should be freed on delete
	Buffer->bDynamicResource = Wave->bDynamicResource;

	Buffer->InitWaveFormatEx( WAVE_FORMAT_PCM, Wave, TRUE );

	XAudio2Device->TrackResource( Wave, Buffer );

	return( Buffer );
}

/**
 * Static function used to create an OpenAL buffer and upload decompressed ogg vorbis data to.
 *
 * @param InWave		USoundNodeWave to use as template and wave source
 * @param AudioDevice	audio device to attach created buffer to
 * @return FALSoundBuffer pointer if buffer creation succeeded, NULL otherwise
 */
FXAudio2SoundBuffer* FXAudio2SoundBuffer::CreateNativeBuffer( UXAudio2Device* XAudio2Device, USoundNodeWave* Wave )
{
#if _WINDOWS
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
#endif

	// Create new buffer.
	FXAudio2SoundBuffer* Buffer = new FXAudio2SoundBuffer( XAudio2Device, SoundFormat_PCM );

	// Take ownership the PCM data
	Buffer->PCM.PCMData = Wave->RawPCMData;
	Buffer->PCM.PCMDataSize = Wave->RawPCMDataSize;

	Wave->RawPCMData = NULL;

	// Keep track of associated resource name.
	Buffer->InitWaveFormatEx( WAVE_FORMAT_PCM, Wave, TRUE );

	XAudio2Device->TrackResource( Wave, Buffer );

	Wave->RemoveAudioResource();

	return( Buffer );
}

/** 
 * Gets the type of buffer that will be created for this wave and stores it.
 */
void FXAudio2SoundBuffer::GetSoundFormat( USoundNodeWave* Wave, FLOAT MinOggVorbisDuration )
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
#if _WINDOWS
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
#elif XBOX
	else
	{
		Wave->DecompressionType = DTYPE_Xenon;
	}
#endif
}

/**
 * Static function used to create a buffer.
 *
 * @param InWave USoundNodeWave to use as template and wave source
 * @param AudioDevice audio device to attach created buffer to
 * @return FXAudio2SoundBuffer pointer if buffer creation succeeded, NULL otherwise
 */
FXAudio2SoundBuffer* FXAudio2SoundBuffer::Init( UAudioDevice* AudioDevice, USoundNodeWave* Wave )
{
	// Can't create a buffer without any source data
	if( Wave == NULL || Wave->NumChannels == 0 )
	{
		return( NULL );
	}

	UXAudio2Device* XAudio2Device = ( UXAudio2Device* )AudioDevice;
	FXAudio2SoundBuffer* Buffer = NULL;

	switch( Wave->DecompressionType )
	{
	case DTYPE_Setup:
		// Has circumvented precache mechanism - precache now
		GetSoundFormat( Wave, GIsEditor ? XAudio2Device->MinCompressedDurationEditor : XAudio2Device->MinCompressedDurationGame );

#if _WINDOWS
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
#endif

		// Recall this function with new decompression type
		return( Init( AudioDevice, Wave ) );

	case DTYPE_Preview:
		// Find the existing buffer if any
		if( Wave->ResourceID )
		{
			Buffer = XAudio2Device->WaveBufferMap.FindRef( Wave->ResourceID );
		}

		// Override with any new PCM data even if some already exists. 
		if( Wave->RawPCMData )
		{
			// Upload the preview PCM data to it
			Buffer = CreatePreviewBuffer( XAudio2Device, Wave, Buffer );
		}
		break;

	case DTYPE_Procedural:
		// Always create a new buffer for streaming procedural data
		Buffer = CreateProceduralBuffer( XAudio2Device, Wave );
		break;

	case DTYPE_RealTime:
		// Always create a new buffer for streaming ogg vorbis data
		Buffer = CreateQueuedBuffer( XAudio2Device, Wave );
		break;

	case DTYPE_Native:
	case DTYPE_Xenon:
		// Upload entire wav to XAudio2
		if( Wave->ResourceID )
		{
			Buffer = XAudio2Device->WaveBufferMap.FindRef( Wave->ResourceID );
		}

		if( Buffer == NULL )
		{
#if _WINDOWS
			Buffer = CreateNativeBuffer( XAudio2Device, Wave );
#elif XBOX
			Buffer = InitXenon( XAudio2Device, Wave );
#endif
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

	return( Buffer );
}

// end

