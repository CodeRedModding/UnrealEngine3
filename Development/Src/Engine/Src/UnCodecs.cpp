/*=============================================================================
	UnCodecs.cpp: Movie codec implementations, e.g. Theora and Fallback codec.
	Copyright 2004 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Daniel Vogel
=============================================================================*/

#include "EnginePrivate.h"
#include "UnCodecs.h"

IMPLEMENT_CLASS(UCodecMovie);
IMPLEMENT_CLASS(UCodecMovieFallback);
IMPLEMENT_CLASS(UCodecMovieTheora);


//
//	UCodecMovieTheora implementation.
//

#ifndef XBOX

// @todo xenon: port theora codec pending needs and availability of xmv
// @todo movie textures: replace blocking implementation using fread/fclose/fopen with non- blocking
// @todo movie textures: implementation that uses a yet to be designed streaming IO manager.

/**
 * Returns the movie width.
 *
 * @return width of movie.
 */
UINT UCodecMovieTheora::GetSizeX() 
{ 
	check(Initialized);
	return TheoraInfo.width; //@todo movie textures: do we want to use the frame_width instead?
}

/**
 * Returns the movie height.
 *
 * @return height of movie.
 */
UINT UCodecMovieTheora::GetSizeY() 
{ 
	check(Initialized);
	return TheoraInfo.height; //@todo movie textures: do we want to use the frame_height instead?
}

/** 
 * Returns the movie format, PF_UYVY if supported, PF_A8R8G8B8 otherwise.
 *
 * @return texture format used for movie.
 */
EPixelFormat UCodecMovieTheora::GetFormat() 
{ 
	check(Initialized);
	return GPixelFormats[PF_UYVY].Supported ? PF_UYVY : PF_A8R8G8B8;
}

/**
 * Returns the framerate the movie was encoded at.
 *
 * @return framerate the movie was encoded at.
 */
FLOAT UCodecMovieTheora::GetFrameRate()
{
	check(Initialized);
	return (FLOAT) TheoraInfo.fps_numerator / TheoraInfo.fps_denominator;
}

/**
 * Reads an Ogg packet from either memory or disk and consumes it via the ogg stream
 * handling.
 *
 * @return the amount of bytes read.
 */
INT UCodecMovieTheora::ReadData()
{
	DWORD ReadSize	= 4096;
	char* Buffer	= ogg_sync_buffer( &OggSyncState, ReadSize );

	ReadSize = Min( ReadSize, BytesRemaining );
	if( MemorySource )
	{
		appMemcpy( Buffer, MemorySource, ReadSize );
		MemorySource += ReadSize;
	}
	else
	{
		ReadSize = fread( Buffer, 1, ReadSize, FileHandle );
	}
	BytesRemaining -= ReadSize;

	ogg_sync_wrote( &OggSyncState, ReadSize );
	return ReadSize;
}

/**
 * Queries whether we're out of data/ for the end of the source stream.
 *	
 * @return TRUE if the end of the source stream has been reached, FALSE otherwise.
 */
UBOOL UCodecMovieTheora::IsEndOfBuffer()
{
	return BytesRemaining <= 0;
}

/**
 * Initializes the decoder to stream from disk.
 *
 * @param	Filename	Filename of compressed media.
 * @param	Offset		Offset into file to look for the beginning of the compressed data.
 * @param	Size		Size of compressed data.
 *
 * @return	TRUE if initialization was successful, FALSE otherwise.
 */
UBOOL UCodecMovieTheora::Open( const FString& Filename, DWORD Offset, DWORD Size )
{
	FileHandle = fopen( TCHAR_TO_ANSI(*Filename), "rb" );
	if( FileHandle == NULL )
	{
		// Error.
		return FALSE;
	}
	fseek( FileHandle, Offset, SEEK_SET );

	OrigOffset				= Offset;
	BytesRemaining			= Size;
	OrigCompressedSize		= Size;

	MemorySource			= NULL;
	OrigSource				= NULL;

	return InitDecoder();
}

/**
 * Initializes the decoder to stream from memory.
 *
 * @param	Source		Beginning of memory block holding compressed data.
 * @param	Size		Size of memory block.
 *
 * @return	TRUE if initialization was successful, FALSE otherwise.
 */
UBOOL UCodecMovieTheora::Open( void* Source, DWORD Size )
{
	MemorySource			= (BYTE*) Source;
	OrigSource				= (BYTE*) Source;
	BytesRemaining			= Size;
	OrigCompressedSize		= Size;

	FileHandle				= NULL;
	OrigOffset				= 0;

	return InitDecoder();
}

/**
 * Resets the stream to its initial state so it can be played again from the beginning.
 */
void UCodecMovieTheora::ResetStream()
{
	check(Initialized);

	ogg_stream_clear( &OggStreamState );
	theora_clear( &TheoraDecoder );
	theora_comment_clear( &TheoraComment );
	theora_info_clear( &TheoraInfo );
	ogg_sync_clear( &OggSyncState );

	if( MemorySource )
	{
		MemorySource = OrigSource;
	}
	else
	{
		fseek( FileHandle, OrigOffset, SEEK_SET );
	}

	BytesRemaining = OrigCompressedSize;

	InitDecoder();
}

/**
 * Inits the decoder by parsing the first couple of packets in the stream. This function
 * contains the shared initialization code and is used by both Open functions.
 *
 * @return TRUE if decoder was successfully initialized, FALSE otherwise
 */
UBOOL UCodecMovieTheora::InitDecoder()
{
	appMemzero( &OggPacket		, sizeof(OggPacket)			);
	appMemzero( &OggSyncState	, sizeof(OggSyncState)		);
	appMemzero( &OggPage		, sizeof(OggPage)			);
	appMemzero( &OggStreamState	, sizeof(OggStreamState)	);
	appMemzero( &TheoraInfo		, sizeof(TheoraInfo)		);
	appMemzero( &TheoraComment	, sizeof(TheoraComment)		);
	appMemzero( &TheoraDecoder	, sizeof(TheoraDecoder)		);	

	ogg_sync_init( &OggSyncState );

	theora_comment_init( &TheoraComment );
	theora_info_init( &TheoraInfo );

	UBOOL DoneParsing	= 0;
	DWORD PacketCount	= 0;
	Initialized			= 0;
	CurrentFrameTime	= 0.f;

	while( !DoneParsing )
	{
		if( ReadData() == 0 )
			break;

		while( ogg_sync_pageout( &OggSyncState, &OggPage ) > 0 )
		{
			ogg_stream_state TestStream;

			if( !ogg_page_bos( &OggPage ) )
			{				
				if(PacketCount)
				{				
					ogg_stream_pagein( &OggStreamState, &OggPage );
				}
				DoneParsing = 1;
				break;
			}

			ogg_stream_init( &TestStream, ogg_page_serialno( &OggPage) );
			ogg_stream_pagein( &TestStream, &OggPage );
			ogg_stream_packetout( &TestStream, &OggPacket ) ;

			if( PacketCount == 0 && theora_decode_header( &TheoraInfo, &TheoraComment, &OggPacket ) >=0 )
			{
				memcpy( &OggStreamState, &TestStream, sizeof(TestStream) ); //@todo movie textures: we're using memcpy here instead of appMemcpy as I know memcpy is thread safe
				PacketCount = 1;
			}
			else
			{
				ogg_stream_clear( &TestStream );
			}
		}
	}

	while( PacketCount && PacketCount < 3 )
	{
		INT RetVal;
		while( PacketCount < 3 && (RetVal=ogg_stream_packetout( &OggStreamState, &OggPacket )) != 0 )
		{
			if( RetVal < 0 )
				return FALSE;

			if( theora_decode_header( &TheoraInfo, &TheoraComment, &OggPacket ) )
				return FALSE;

			PacketCount++;
		}


		if( ogg_sync_pageout( &OggSyncState, &OggPage ) > 0 )
		{
			ogg_stream_pagein( &OggStreamState, &OggPage );
		}
		else if( ReadData() == 0 )
		{
			return FALSE;		
		}
	}

	
	if( PacketCount )
	{
		theora_decode_init( &TheoraDecoder, &TheoraInfo );

		while( ogg_sync_pageout( &OggSyncState, &OggPage ) > 0 )
			ogg_stream_pagein( &OggStreamState, &OggPage );

		Initialized = 1;
		
		return TRUE;
	}
	else
	{
		theora_info_clear( &TheoraInfo );
		theora_comment_clear( &TheoraComment );

		return FALSE;
	}
}

/**
 * Tears down stream by calling Close.	
 */
void UCodecMovieTheora::Destroy()
{
	Super::Destroy();
	Close();
}

/**
 * Tears down stream, closing file handle if there was an open one.	
 */
void UCodecMovieTheora::Close()
{
	if( Initialized )
	{
		ogg_stream_clear( &OggStreamState );
		theora_clear( &TheoraDecoder );
		theora_comment_clear( &TheoraComment );
		theora_info_clear( &TheoraInfo );

		ogg_sync_clear( &OggSyncState );

		if( FileHandle )
			fclose( FileHandle );

		Initialized		= 0;

		MemorySource	= NULL;
		BytesRemaining	= 0;
		FileHandle		= NULL;
	}
}

/**
 * Queues the request to retrieve the next frame.
 *
 * @param	Destination		Memory block to uncompress data into.
 */
UBOOL UCodecMovieTheora::GetFrame( void* Dest )
{
	UBOOL	EnoughData	= 0;
	while( !EnoughData )
	{
		while( !EnoughData )
		{
			if( ogg_stream_packetout( &OggStreamState, &OggPacket) > 0 )
			{
				theora_decode_packetin( &TheoraDecoder, &OggPacket );
				ogg_int64_t GranulePos	= TheoraDecoder.granulepos;
				CurrentFrameTime		= theora_granule_time( &TheoraDecoder, GranulePos );				
				EnoughData				= 1;
			}
			else
				break;
		}

		if( !EnoughData )	
		{
			if( IsEndOfBuffer() )
			{		
				return FALSE;
			}
			else
			{
				ReadData();
					
				while( ogg_sync_pageout( &OggSyncState, &OggPage ) > 0 )
					ogg_stream_pagein( &OggStreamState, &OggPage );
			}
		}
	}
	
	yuv_buffer YUVBuffer;
	theora_decode_YUVout( &TheoraDecoder, &YUVBuffer );

	DWORD	SizeX	= TheoraInfo.width,
			SizeY	= TheoraInfo.height;

	char*	SrcY	= YUVBuffer.y;
	char*	SrcU	= YUVBuffer.u;
	char*	SrcV	= YUVBuffer.v;

	check( YUVBuffer.y_width  == YUVBuffer.uv_width  * 2 );
	check( YUVBuffer.y_height == YUVBuffer.uv_height * 2 );
	check( YUVBuffer.y_width  == GetSizeX() );
	check( YUVBuffer.y_height == GetSizeY() );

	if( GPixelFormats[PF_UYVY].Supported )
	{
		//@todo optimization: Converts from planar YUV to interleaved PF_UYVY format.
		for( UINT y=0; y<SizeY; y++ )
		{
			for( UINT x=0; x<SizeX/2; x++ )
			{
				DWORD			OffsetY0	= YUVBuffer.y_stride  *  y    + x*2;
				DWORD			OffsetY1	= YUVBuffer.y_stride  *  y    + x*2 + 1;
				DWORD			OffsetUV	= YUVBuffer.uv_stride * (y/2) + x;

				unsigned char	Y0			= SrcY[OffsetY0];
				unsigned char   Y1			= SrcY[OffsetY1];
				unsigned char	U			= SrcU[OffsetUV];
				unsigned char	V			= SrcV[OffsetUV];

				((DWORD*)Dest)[y*(SizeX/2) + x] = (Y1 << 24) | (V << 16) | (Y0 << 8) | U; //@todo xenon: I bet this needs to be shuffled around.
			}
		}
	}
	else
	{
		FColor*	DestColor = (FColor*) Dest;

		//@todo optimization: Converts from planar YUV to interleaved ARGB. This codepath is currently hit with NVIDIA cards!
		for( UINT y=0; y<SizeY; y++ )
		{
			for( UINT x=0; x<SizeX/2; x++ )
			{
				DWORD			OffsetY0	= YUVBuffer.y_stride  *  y    + x*2;
				DWORD			OffsetY1	= YUVBuffer.y_stride  *  y    + x*2 + 1;
				DWORD			OffsetUV	= YUVBuffer.uv_stride * (y/2) + x;

				unsigned char	Y0			= SrcY[OffsetY0];
				unsigned char   Y1			= SrcY[OffsetY1];
				unsigned char	U			= SrcU[OffsetUV];
				unsigned char	V			= SrcV[OffsetUV];

				//@todo optimization: this is a prime candidate for SSE/ Altivec or fixed point integer or moving it to the GPU though we really ought to just avoid this codepath altogether.
				DestColor->R				= Clamp( appTrunc( Y0 + 1.402f * (V-128) ), 0, 255 );
				DestColor->G				= Clamp( appTrunc( Y0 - 0.34414f * (U-128) - 0.71414f * (V-128) ), 0, 255 );
				DestColor->B				= Clamp( appTrunc( Y0 + 1.772 * (U-128) ), 0, 255 );
				DestColor->A				= 255;
				DestColor++;

				//@todo optimization: this is a prime candidate for SSE/ Altivec or fixed point integer or moving it to the GPU though we really ought to just avoid this codepath altogether.
				DestColor->R				= Clamp( appTrunc( Y1 + 1.402f * (V-128) ), 0, 255 );
				DestColor->G				= Clamp( appTrunc( Y1 - 0.34414f * (U-128) - 0.71414f * (V-128) ), 0, 255 );
				DestColor->B				= Clamp( appTrunc( Y1 + 1.772 * (U-128) ), 0, 255 );
				DestColor->A				= 255;
				DestColor++;
			}
		}

	}
	return TRUE;
}

/**
 * Blocks until the decoder has finished the pending decompression request.
 *
 * @return	Time into movie playback as seen from the decoder side.
 */
FLOAT UCodecMovieTheora::BlockUntilIdle() 
{ 
	return CurrentFrameTime; 
}	

#endif

//
//	UCodecMovieFallback implementation.
//

/**
 * Returns the movie width.
 *
 * @return width of movie.
 */
UINT UCodecMovieFallback::GetSizeX()
{
	return 1;
}

/**
 * Returns the movie height.
 *
 * @return height of movie.
 */
UINT UCodecMovieFallback::GetSizeY()
{
	return 1;
}

/** 
 * Returns the movie format, in this case PF_A8R8G8B8
 *
 * @return format of movie (always PF_A8R8G8B8)
 */
EPixelFormat UCodecMovieFallback::GetFormat()
{
	return PF_A8R8G8B8;
}

/**
 * Initializes the decoder to stream from disk.
 *
 * @param	Filename	unused
 * @param	Offset		unused
 * @param	Size		unused.
 *
 * @return	TRUE if initialization was successful, FALSE otherwise.
 */
UBOOL UCodecMovieFallback::Open( const FString& /*Filename*/, DWORD /*Offset*/, DWORD /*Size*/ )
{
	PlaybackDuration	= 1.f;
	CurrentTime			= 0;
	return TRUE;
}

/**
 * Initializes the decoder to stream from memory.
 *
 * @param	Source		unused
 * @param	Size		unused
 *
 * @return	TRUE if initialization was successful, FALSE otherwise.
 */
UBOOL UCodecMovieFallback::Open( void* /*Source*/, DWORD /*Size*/ )
{
	PlaybackDuration	= 1.f;
	CurrentTime			= 0;
	return TRUE;
}

/**
 * Tears down stream, closing file handle if there was an open one.	
 */
void UCodecMovieFallback::Close()
{
}

/**
 * Resets the stream to its initial state so it can be played again from the beginning.
 */
void UCodecMovieFallback::ResetStream()
{
	CurrentTime = 0.f;
}

/**
 * Blocks until the decoder has finished the pending decompression request.
 *
 * @return	Time into movie playback as seen from the decoder side.
 */
FLOAT UCodecMovieFallback::BlockUntilIdle() 
{ 
	return CurrentTime;
}		

/**
 * Returns the framerate the movie was encoded at.
 *
 * @return framerate the movie was encoded at.
 */
FLOAT UCodecMovieFallback::GetFrameRate()
{
	return 10.f;
}

/**
 * Queues the request to retrieve the next frame.
 *
 * @param	Destination		Memory block to uncompress data into.
 * @return	FALSE if the end of the frame has been reached and the frame couldn't be completed, TRUE otherwise
 */
UBOOL UCodecMovieFallback::GetFrame( void* Destination )
{
	CurrentTime += 1.f / GetFrameRate();

	if( CurrentTime > PlaybackDuration )
	{
		return FALSE;
	}
	else
	{
		*((FColor*) Destination) = FColor( 255, 255 * appFractional(PlaybackDuration / CurrentTime), 0 );
	}
	return TRUE;
}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
