/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#include "EnginePrivate.h"
#include "EngineSoundClasses.h"

#include "UnConsoleTools.h"
#include "UnAudioDecompress.h"

#if __INTEL_BYTE_ORDER__
#define VORBIS_BYTE_ORDER 0
#else
#define VORBIS_BYTE_ORDER 1
#endif


/*------------------------------------------------------------------------------------
	FVorbisAudioInfo.
------------------------------------------------------------------------------------*/

/** Emulate read from memory functionality */
size_t FVorbisAudioInfo::Read( void *Ptr, DWORD Size )
{
	size_t BytesToRead = Min( Size, SrcBufferDataSize - BufferOffset );
	appMemcpy( Ptr, SrcBufferData + BufferOffset, BytesToRead );
	BufferOffset += BytesToRead;
	return( BytesToRead );
}

static size_t OggRead( void *ptr, size_t size, size_t nmemb, void *datasource )
{
	FVorbisAudioInfo* OggInfo = ( FVorbisAudioInfo* )datasource;
	return( OggInfo->Read( ptr, size * nmemb ) );
}

int FVorbisAudioInfo::Seek( DWORD offset, int whence )
{
	switch( whence )
	{
	case SEEK_SET:
		BufferOffset = offset;
		break;

	case SEEK_CUR:
		BufferOffset += offset;
		break;

	case SEEK_END:
		BufferOffset = SrcBufferDataSize - offset;
		break;
	}

	return( BufferOffset );
}


#if WITH_OGGVORBIS

static int OggSeek( void *datasource, ogg_int64_t offset, int whence )
{
	FVorbisAudioInfo* OggInfo = ( FVorbisAudioInfo* )datasource;
	return( OggInfo->Seek( offset, whence ) );
}

int FVorbisAudioInfo::Close( void )
{
	return( 0 );
}

static int OggClose( void *datasource )
{
	FVorbisAudioInfo* OggInfo = ( FVorbisAudioInfo* )datasource;
	return( OggInfo->Close() );
}

long FVorbisAudioInfo::Tell( void )
{
	return( BufferOffset );
}

static long OggTell( void *datasource )
{
	FVorbisAudioInfo *OggInfo = ( FVorbisAudioInfo* )datasource;
	return( OggInfo->Tell() );
}

#endif		// WITH_OGGVORBIS


#if WITH_OGGVORBIS

/** 
 * Reads the header information of an ogg vorbis file
 * 
 * @param	Resource		Info about vorbis data
 */
UBOOL FVorbisAudioInfo::ReadCompressedInfo( BYTE* InSrcBufferData, DWORD InSrcBufferDataSize, FSoundQualityInfo* QualityInfo )
{
	SCOPE_CYCLE_COUNTER( STAT_VorbisPrepareDecompressionTime );

	ov_callbacks		Callbacks;

	SrcBufferData = InSrcBufferData;
	SrcBufferDataSize = InSrcBufferDataSize;
	BufferOffset = 0;

	Callbacks.read_func = OggRead;
	Callbacks.seek_func = OggSeek;
	Callbacks.close_func = OggClose;
	Callbacks.tell_func = OggTell;

	// Set up the read from memory variables
	if (ov_open_callbacks(this, &vf, NULL, 0, Callbacks) < 0)
	{
		return( FALSE );
	}

	if( QualityInfo )
	{
		// The compression could have resampled the source to make loopable
		vorbis_info* vi = ov_info( &vf, -1 );
		QualityInfo->SampleRate = vi->rate;
		QualityInfo->NumChannels = vi->channels;
		QualityInfo->SampleDataSize = ov_pcm_total( &vf, -1 ) * QualityInfo->NumChannels * sizeof( SWORD );
		QualityInfo->Duration = ( FLOAT )ov_time_total( &vf, -1 );
	}

	return( TRUE );
}


/** 
 * Decompress an entire ogg vorbis data file to a TArray
 */
void FVorbisAudioInfo::ExpandFile( BYTE* DstBuffer, FSoundQualityInfo* QualityInfo )
{
	DWORD		BytesRead, TotalBytesRead, BytesToRead;

	check( DstBuffer );
	check( QualityInfo );

	// A zero buffer size means decompress the entire ogg vorbis stream to PCM.
	TotalBytesRead = 0;
	BytesToRead = QualityInfo->SampleDataSize;

	char* Destination = ( char* )DstBuffer;
	while( TotalBytesRead < BytesToRead )
	{
		BytesRead = ov_read( &vf, Destination, BytesToRead - TotalBytesRead, 0, 2, 1, NULL );

		TotalBytesRead += BytesRead;
		Destination += BytesRead;
	}
}



/** 
 * Decompresses ogg vorbis data to raw PCM data. 
 * 
 * @param	PCMData		where to place the decompressed sound
 * @param	bLooping	whether to loop the wav by seeking to the start, or pad the buffer with zeroes
 * @param	BufferSize	number of bytes of PCM data to create. A value of 0 means decompress the entire sound.
 *
 * @return	UBOOL		TRUE if the end of the data was reached (for both single shot and looping sounds)
 */
UBOOL FVorbisAudioInfo::ReadCompressedData( const BYTE* InDestination, UBOOL bLooping, DWORD BufferSize )
{
	UBOOL		bLooped;
	DWORD		BytesRead, TotalBytesRead;

	SCOPE_CYCLE_COUNTER( STAT_VorbisDecompressTime );

	bLooped = FALSE;

	// Work out number of samples to read
	TotalBytesRead = 0;
	char* Destination = ( char* )InDestination;

	while( TotalBytesRead < BufferSize )
	{
		BytesRead = ov_read( &vf, Destination, BufferSize - TotalBytesRead, 0, 2, 1, NULL );
		if( !BytesRead )
		{
			// We've reached the end
			bLooped = TRUE;
			if( bLooping )
			{
				ov_pcm_seek_page( &vf, 0 );
			}
			else
			{
				INT Count = ( BufferSize - TotalBytesRead );
				appMemzero( Destination, Count );

				BytesRead += BufferSize - TotalBytesRead;
			}
		}

		TotalBytesRead += BytesRead;
		Destination += BytesRead;
	}

	return( bLooped );
}

#endif		// WITH_OGGVORBIS

/**
 * Worker for decompression on a separate thread
 */
void FAsyncVorbisDecompressWorker::DoWork( void )
{
	FVorbisAudioInfo	OggInfo;
	FSoundQualityInfo	QualityInfo = { 0 };

	// Parse the ogg vorbis header for the relevant information
	if( OggInfo.ReadCompressedInfo( Wave->ResourceData, Wave->ResourceSize, &QualityInfo ) )
	{
		// Extract the data
		Wave->SampleRate = QualityInfo.SampleRate;
		Wave->NumChannels = QualityInfo.NumChannels;
		Wave->Duration = QualityInfo.Duration;

		Wave->RawPCMDataSize = QualityInfo.SampleDataSize;
		Wave->RawPCMData = ( BYTE* )appMalloc( Wave->RawPCMDataSize );

		// Decompress all the sample data into preallocated memory
		OggInfo.ExpandFile( Wave->RawPCMData, &QualityInfo );

		INC_DWORD_STAT_BY( STAT_AudioMemorySize, Wave->GetResourceSize() );
		INC_DWORD_STAT_BY( STAT_AudioMemory, Wave->GetResourceSize() );
	}

	// Delete the compressed data
	Wave->RemoveAudioResource();
}

// end
