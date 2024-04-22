/*=============================================================================
	UnAudioDecompress.h: Unreal audio vorbis decompression interface object.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef _INC_UNAUDIODECOMPRESS
#define _INC_UNAUDIODECOMPRESS

// 186ms of 44.1KHz data
// 372ms of 22KHz data
#define MONO_PCM_BUFFER_SAMPLES		8192
#define MONO_PCM_BUFFER_SIZE		( MONO_PCM_BUFFER_SAMPLES * sizeof( SWORD ) )

#if PLATFORM_DESKTOP
#if SUPPORTS_PRAGMA_PACK
#pragma pack( push, 8 )
#endif

#if WITH_OGGVORBIS
#include <vorbis/vorbisenc.h>
#include <vorbis/vorbisfile.h>
#endif

#if SUPPORTS_PRAGMA_PACK
#pragma pack( pop )
#endif
#endif

/** 
 * Helper class to parse ogg vorbis data
 */
class FVorbisAudioInfo
{
public:
	FVorbisAudioInfo( void ) 
	{ 
		SrcBufferData = NULL;
		SrcBufferDataSize = 0; 
#if WITH_OGGVORBIS && PLATFORM_DESKTOP
		appMemzero( &vf, sizeof( OggVorbis_File ) );
#endif
	}

	~FVorbisAudioInfo( void ) 
	{ 
#if WITH_OGGVORBIS && PLATFORM_DESKTOP
		ov_clear( &vf ); 
#endif
	}

	/** Emulate read from memory functionality */
	size_t			Read( void *ptr, DWORD size );
	int				Seek( DWORD offset, int whence );
	int				Close( void );
	long			Tell( void );

	/** 
	 * Reads the header information of an ogg vorbis file
	 * 
	 * @param	Resource		Info about vorbis data
	 */
#if WITH_OGGVORBIS && PLATFORM_DESKTOP
	UBOOL ReadCompressedInfo( BYTE* InSrcBufferData, DWORD InSrcBufferDataSize, struct FSoundQualityInfo* QualityInfo );
#else
	UBOOL ReadCompressedInfo( BYTE* InSrcBufferData, DWORD InSrcBufferDataSize, struct FSoundQualityInfo* QualityInfo )
	{
		return( FALSE );
	}
#endif

	/** 
	 * Decompresses ogg data to raw PCM data. 
	 * 
	 * @param	PCMData		where to place the decompressed sound
	 * @param	bLooping	whether to loop the sound by seeking to the start, or pad the buffer with zeroes
	 * @param	BufferSize	number of bytes of PCM data to create
	 *
	 * @return	UBOOL		TRUE if the end of the data was reached (for both single shot and looping sounds)
	 */
#if WITH_OGGVORBIS && PLATFORM_DESKTOP
	UBOOL ReadCompressedData( const BYTE* Destination, UBOOL bLooping, DWORD BufferSize = 0 );
#else
	UBOOL ReadCompressedData( const BYTE* Destination, UBOOL bLooping, DWORD BufferSize = 0 )
	{
		return( FALSE );
	}
#endif

	/** 
	 * Decompress an entire ogg data file to a TArray
	 */
#if WITH_OGGVORBIS && PLATFORM_DESKTOP
	void ExpandFile( BYTE* DstBuffer, struct FSoundQualityInfo* QualityInfo );
#else
	void ExpandFile( BYTE* DstBuffer, struct FSoundQualityInfo* QualityInfo )
	{
	}
#endif

#if WITH_OGGVORBIS && PLATFORM_DESKTOP
	/** Ogg vorbis decompression state */
	OggVorbis_File	vf;
#endif

	const BYTE*		SrcBufferData;
	DWORD			SrcBufferDataSize;
	DWORD			BufferOffset;
};

#endif
