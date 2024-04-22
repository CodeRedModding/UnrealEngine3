/*=============================================================================
	UnAudioCompress.h: Unreal audio compression - ogg vorbis
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h" 

#include "UnConsoleTools.h"
#include "UnAudioDecompress.h"
#include "UnAudioCompress.h"

// Vorbis encoded sound is about 15% better quality than XMA - adjust the quality setting to get consistent cross platform sound quality
#define VORBIS_QUALITY_MODIFIER		-15

#define SAMPLES_TO_READ		1024
#define SAMPLE_SIZE			( ( DWORD )sizeof( short ) )

/**
 * Create ogg vorbis compressed data from raw PCM data at a given frequency
 */
bool FPCSoundCooker::Cook( const BYTE* SrcBuffer, FSoundQualityInfo* QualityInfo )
{
#if WITH_OGGVORBIS
	short				ReadBuffer[SAMPLES_TO_READ * SAMPLE_SIZE * 2];

	ogg_stream_state	os;		// take physical pages, weld into a logical stream of packets 
	ogg_page			og;		// one ogg bitstream page.  Vorbis packets are inside
	ogg_packet			op;		// one raw packet of data for decode
	vorbis_info			vi;		// struct that stores all the static vorbis bitstream settings
	vorbis_comment		vc;		// struct that stores all the user comments
	vorbis_dsp_state	vd;		// central working state for the packet->PCM decoder
	vorbis_block		vb;		// local working space for packet->PCM decode
	DWORD				i;
	bool				eos;

	// Create a buffer to store compressed data
	CompressedDataStore.Empty();
	FMemoryWriter* CompressedData = new FMemoryWriter( CompressedDataStore );
	BufferOffset = 0;

	float CompressionQuality = ( float )( QualityInfo->Quality + VORBIS_QUALITY_MODIFIER ) / 100.0f;
	CompressionQuality = Clamp( CompressionQuality, -0.1f, 1.0f );

	vorbis_info_init( &vi );

	if( vorbis_encode_init_vbr( &vi, QualityInfo->NumChannels, QualityInfo->SampleRate, CompressionQuality ) )
	{
		return( false );
	}

	// add a comment
	vorbis_comment_init( &vc );
	vorbis_comment_add_tag( &vc, "ENCODER", "UnrealEngine3" );

	// set up the analysis state and auxiliary encoding storage
	vorbis_analysis_init( &vd, &vi );
	vorbis_block_init( &vd, &vb );

	// set up our packet->stream encoder
	ogg_stream_init( &os, 0 );

	ogg_packet header;
	ogg_packet header_comm;
	ogg_packet header_code;

	vorbis_analysis_headerout( &vd, &vc, &header, &header_comm, &header_code);
	ogg_stream_packetin( &os, &header );
	ogg_stream_packetin( &os, &header_comm );
	ogg_stream_packetin( &os, &header_code );

	// This ensures the actual audio data will start on a new page, as per spec
	while( true )
	{
		int result = ogg_stream_flush( &os, &og );
		if( result == 0 )
		{
			break;
		}

		CompressedData->Serialize( og.header, og.header_len );
		CompressedData->Serialize( og.body, og.body_len );
	}

	eos = false;
	while( !eos )
	{
		// Read samples
		DWORD BytesToRead = Min( SAMPLES_TO_READ * QualityInfo->NumChannels * SAMPLE_SIZE, QualityInfo->SampleDataSize - BufferOffset );
		appMemcpy( ReadBuffer, SrcBuffer + BufferOffset, BytesToRead );
		BufferOffset += BytesToRead;

		if( BytesToRead == 0)
		{
			// end of file
			vorbis_analysis_wrote( &vd, 0 );
		}
		else
		{
			// expose the buffer to submit data
			float **buffer = vorbis_analysis_buffer( &vd, SAMPLES_TO_READ );

			if( QualityInfo->NumChannels == 1 )
			{
				for( i = 0; i < BytesToRead / SAMPLE_SIZE; i++ )
				{
					buffer[0][i] = ( ReadBuffer[i] ) / 32768.0f;
				}
			}
			else
			{
				for( i = 0; i < BytesToRead / ( SAMPLE_SIZE * 2 ); i++ )
				{
					buffer[0][i] = ( ReadBuffer[i * 2] ) / 32768.0f;
					buffer[1][i] = ( ReadBuffer[i * 2 + 1] ) / 32768.0f;
				}
			}

			// tell the library how many samples we actually submitted
			vorbis_analysis_wrote( &vd, i );
		}

		// vorbis does some data preanalysis, then divvies up blocks for more involved (potentially parallel) processing.
		while( vorbis_analysis_blockout( &vd, &vb ) == 1 )
		{
			// analysis, assume we want to use bitrate management
			vorbis_analysis( &vb, NULL );
			vorbis_bitrate_addblock( &vb );

			while( vorbis_bitrate_flushpacket( &vd, &op ) )
			{
				// weld the packet into the bitstream
				ogg_stream_packetin( &os, &op );

				// write out pages (if any)
				while( !eos )
				{
					int result = ogg_stream_pageout( &os, &og );
					if( result == 0 )
					{
						break;
					}
					CompressedData->Serialize( og.header, og.header_len );
					CompressedData->Serialize( og.body, og.body_len );

					// this could be set above, but for illustrative purposes, I do	it here (to show that vorbis does know where the stream ends)
					if( ogg_page_eos( &og ) )
					{
						eos = true;
					}
				}
			}
		}
	}

	// clean up and exit.  vorbis_info_clear() must be called last
	ogg_stream_clear( &os );
	vorbis_block_clear( &vb );
	vorbis_dsp_clear( &vd );
	vorbis_comment_clear( &vc );
	vorbis_info_clear( &vi );

	// ogg_page and ogg_packet structs always point to storage in libvorbis.  They're never freed or manipulated directly
	return( true );
#else
	return false;
#endif		// WITH_OGGVOBVIS
}

/**
 * Cooks upto 8 mono files into a multistream file (eg. 5.1). The front left channel is required, the rest are optional.
 *
 * @param	SrcBuffers		Pointers to source buffers
 * @param	SrcBufferSize	Size in bytes of source buffer
 * @param	WaveFormat		Pointer to platform specific wave format description
 * @param	Compression		Quality value ranging from 1 [poor] to 100 [very good]
 *
 * @return	true if succeeded, false otherwise
 */
bool FPCSoundCooker::CookSurround( const BYTE* SrcBuffers[8], FSoundQualityInfo* QualityInfo )
{
#if WITH_OGGVORBIS

	ogg_stream_state	os;		// take physical pages, weld into a logical stream of packets 
	ogg_page			og;		// one ogg bitstream page.  Vorbis packets are inside
	ogg_packet			op;		// one raw packet of data for decode
	vorbis_info			vi;		// struct that stores all the static vorbis bitstream settings
	vorbis_comment		vc;		// struct that stores all the user comments
	vorbis_dsp_state	vd;		// central working state for the packet->PCM decoder
	vorbis_block		vb;		// local working space for packet->PCM decode
	DWORD				i;
	bool				eos;
	int					j;
	int					ChannelCount;

	// Create a buffer to store compressed data
	CompressedDataStore.Empty();
	FMemoryWriter* CompressedData = new FMemoryWriter( CompressedDataStore );
	BufferOffset = 0;

	// Extract the relevant info for compression
	float CompressionQuality = ( float )( QualityInfo->Quality + VORBIS_QUALITY_MODIFIER ) / 100.0f;
	CompressionQuality = Clamp( CompressionQuality, 0.0f, 1.0f );

	// Count up the number of active channels
	ChannelCount = 0;
	for( i = 0; i < 8; i++ )
	{
		if( SrcBuffers[i] )
		{
			ChannelCount++;
		}
	}

	vorbis_info_init( &vi );

	if( vorbis_encode_init_vbr( &vi, ChannelCount, QualityInfo->SampleRate, CompressionQuality ) )
	{
		return( false );
	}

	// add a comment
	vorbis_comment_init( &vc );
	vorbis_comment_add_tag( &vc, "ENCODER", "UnrealEngine3" );

	// set up the analysis state and auxiliary encoding storage
	vorbis_analysis_init( &vd, &vi );
	vorbis_block_init( &vd, &vb );

	// set up our packet->stream encoder
	ogg_stream_init( &os, 0 );

	ogg_packet header;
	ogg_packet header_comm;
	ogg_packet header_code;

	vorbis_analysis_headerout( &vd, &vc, &header, &header_comm, &header_code);
	ogg_stream_packetin( &os, &header );
	ogg_stream_packetin( &os, &header_comm );
	ogg_stream_packetin( &os, &header_code );

	// This ensures the actual audio data will start on a new page, as per spec
	while( true )
	{
		int result = ogg_stream_flush( &os, &og );
		if( result == 0 )
		{
			break;
		}

		CompressedData->Serialize( og.header, og.header_len );
		CompressedData->Serialize( og.body, og.body_len );
	}

	eos = false;
	while( !eos )
	{
		// Read samples
		DWORD BytesToRead = Min( SAMPLES_TO_READ * SAMPLE_SIZE, QualityInfo->SampleDataSize - BufferOffset );
		if( BytesToRead == 0)
		{
			// end of file
			vorbis_analysis_wrote( &vd, 0 );
		}
		else
		{
			// expose the buffer to submit data
			float **buffer = vorbis_analysis_buffer( &vd, SAMPLES_TO_READ );

			ChannelCount = 0;
			for( j = 0; j < 8; j++ )
			{
				if( SrcBuffers[j] )
				{
					short* ReadBuffer = ( short* )( SrcBuffers[j] + BufferOffset );
					for( i = 0; i < BytesToRead / SAMPLE_SIZE; i++ )
					{
						buffer[ChannelCount][i] = ( ReadBuffer[i] ) / 32768.0f;
					}

					ChannelCount++;
				}
			}

			// tell the library how many samples we actually submitted
			vorbis_analysis_wrote( &vd, i );
		}
		BufferOffset += BytesToRead;

		// vorbis does some data preanalysis, then divvies up blocks for more involved (potentially parallel) processing.
		while( vorbis_analysis_blockout( &vd, &vb ) == 1 )
		{
			// analysis, assume we want to use bitrate management
			vorbis_analysis( &vb, NULL );
			vorbis_bitrate_addblock( &vb );

			while( vorbis_bitrate_flushpacket( &vd, &op ) )
			{
				// weld the packet into the bitstream
				ogg_stream_packetin( &os, &op );

				// write out pages (if any)
				while( !eos )
				{
					int result = ogg_stream_pageout( &os, &og );
					if( result == 0 )
					{
						break;
					}
					CompressedData->Serialize( og.header, og.header_len );
					CompressedData->Serialize( og.body, og.body_len );

					// this could be set above, but for illustrative purposes, I do	it here (to show that vorbis does know where the stream ends)
					if( ogg_page_eos( &og ) )
					{
						eos = true;
					}
				}
			}
		}
	}

	// clean up and exit.  vorbis_info_clear() must be called last
	ogg_stream_clear( &os );
	vorbis_block_clear( &vb );
	vorbis_dsp_clear( &vd );
	vorbis_comment_clear( &vc );
	vorbis_info_clear( &vi );

	// ogg_page and ogg_packet structs always point to storage in libvorbis.  They're never freed or manipulated directly
	return( true );

#else
	return false;
#endif		// WITH_OGGVORBIS
}

/**
 * Return the number of bytes created by the previous vorbis encoding
 */
UINT FPCSoundCooker::GetCookedDataSize( void )
{
	check( CompressedDataStore.Num() );
	return( ( UINT )CompressedDataStore.Num() );
}

/**
 * Return the encoded vorbis data
 */
void FPCSoundCooker::GetCookedData( BYTE* CookedData )
{
	FMemoryReader Data( CompressedDataStore );
	Data.Serialize( CookedData, Data.TotalSize() );

	CompressedDataStore.Empty();
}

/** 
 * Decompresses the platform dependent format to raw PCM. Used for quality previewing.
 *
 * @param	UncompressedData		PCM data
 * @param	UncompressedDataSize	Size of PCM data
 * @param	CompressedData			ogg vorbis data
 * @param	CompressedDataSize		Size of ogg vorbis data
 */
INT FPCSoundCooker::Recompress( const BYTE* SrcBuffer, BYTE* DestBuffer, FSoundQualityInfo* QualityInfo )
{
	FVorbisAudioInfo	OggInfo;
	INT					CompressedSize = -1;

	// Cannot quality preview multichannel sounds
	if( QualityInfo->NumChannels > 2 )
	{
		return( -1 );
	}

	if( !Cook( SrcBuffer, QualityInfo ) )
	{
		return( -1 );
	}

	CompressedSize = ( INT )CompressedDataStore.Num();

	// Parse the ogg vorbis header for the relevant information
	if( !OggInfo.ReadCompressedInfo( CompressedDataStore.GetTypedData(), CompressedSize, QualityInfo ) )
	{
		return( -1 );
	}

	// Decompress all the sample data
	OggInfo.ExpandFile( DestBuffer, QualityInfo );

	CompressedDataStore.Empty();
	return( CompressedSize );
}

/**
 * Singleton to return the cooking class for PC sounds
 */
FPCSoundCooker* GetPCSoundCooker( void )
{
	static FPCSoundCooker* SoundCooker = NULL;
	if( !SoundCooker )
	{
		SoundCooker = new FPCSoundCooker();
	}

	return( SoundCooker );
}

// end
