/*=============================================================================
	UnCodecs.h: Movie codec definitions.
	Copyright 2004 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Daniel Vogel
=============================================================================*/

#ifndef XBOX

#if SUPPORTS_PRAGMA_PACK
#pragma pack (push,8)
#endif
#include <theora/theora.h>
#if SUPPORTS_PRAGMA_PACK
#pragma pack (pop)
#endif

/**
 * Implementation of Theora <http://www.theora.org> decoder.
 */
class UCodecMovieTheora : public UCodecMovie
{
	DECLARE_CLASS(UCodecMovieTheora,UCodecMovie,0,Engine)

	// UCodecMovie interface.

	/**
	 * Returns the movie width.
	 *
	 * @return width of movie.
	 */
	virtual UINT GetSizeX();
	/**
	 * Returns the movie height.
	 *
	 * @return height of movie.
	 */
	virtual UINT GetSizeY();
	/** 
	 * Returns the movie format,
	 *
	 * @return format of movie.
	 */
	virtual EPixelFormat GetFormat();
	/**
	 * Returns the framerate the movie was encoded at.
	 *
	 * @return framerate the movie was encoded at.
	 */
	virtual FLOAT GetFrameRate();

	/**
	 * Initializes the decoder to stream from disk.
	 *
	 * @param	Filename	Filename of compressed media.
	 * @param	Offset		Offset into file to look for the beginning of the compressed data.
	 * @param	Size		Size of compressed data.
	 *
	 * @return	TRUE if initialization was successful, FALSE otherwise.
	 */
	virtual UBOOL Open( const FString& Filename, DWORD Offset, DWORD Size );
	/**
	 * Initializes the decoder to stream from memory.
	 *
	 * @param	Source		Beginning of memory block holding compressed data.
	 * @param	Size		Size of memory block.
	 *
	 * @return	TRUE if initialization was successful, FALSE otherwise.
	 */
	virtual UBOOL Open( void* Source, DWORD Size );
	/**
	 * Tears down stream, closing file handle if there was an open one.	
	 */
	virtual void Close();

	/**
	 * Resets the stream to its initial state so it can be played again from the beginning.
	 */
	virtual void ResetStream();
	/**
	 * Blocks until the decoder has finished the pending decompression request.
	 *
	 * @return	Time into movie playback as seen from the decoder side.
	 */
	virtual FLOAT BlockUntilIdle();
	/**
	 * Queues the request to retrieve the next frame.
	 *
 	 * @param	Destination		Memory block to uncompress data into.
	 * @return	FALSE if the end of the frame has been reached and the frame couldn't be completed, TRUE otherwise
	 */
	virtual UBOOL GetFrame( void* Destination );


	// UObject interface.

	/**
	 * Tears down stream by calling Close.	
	 */
	virtual void Destroy();

private:
	// Theora/ Ogg status objects.
	ogg_packet			OggPacket;
	ogg_sync_state		OggSyncState;
	ogg_page			OggPage;
	ogg_stream_state	OggStreamState;
	theora_info			TheoraInfo;
	theora_comment		TheoraComment;
	theora_state		TheoraDecoder;

	/** Handle to file we're streaming from, NULL if streaming directly from memory.		*/
	FILE*				FileHandle;
	
	/** Pointer to memory we're streaming from, NULL if streaming from disk.				*/
	BYTE*				MemorySource;
	/** Number of compressed bytes remaining in input stream								*/
	DWORD				BytesRemaining;

	/** Original offset into file as passed to Open.										*/
	DWORD				OrigOffset;
	/** Original size of input stream.														*/
	DWORD				OrigCompressedSize;
	/** Original memory location as passed to Open.											*/
	BYTE*				OrigSource;

	/** Whether the decoder has bee successfully initialized.								*/
	UBOOL				Initialized;

	/** Frame time of current frame.														*/
	FLOAT				CurrentFrameTime;

	/**
	 * Reads an Ogg packet from either memory or disk and consumes it via the ogg stream
	 * handling.
	 *
	 * @return the amount of bytes read.
	 */
	INT ReadData();

	/**
	 * Queries whether we're out of data/ for the end of the source stream.
	 *	
	 * @return TRUE if the end of the source stream has been reached, FALSE otherwise.
	 */
	UBOOL IsEndOfBuffer();

	/**
	 * Inits the decoder by parsing the first couple of packets in the stream. This function
	 * contains the shared initialization code and is used by both Open functions.
	 *
	 * @return TRUE if decoder was successfully initialized, FALSE otherwise
	 */
	UBOOL InitDecoder();
};

#else

/**
 * Dummy implementation for Xenon.	
 */
class UCodecMovieTheora : public UCodecMovie
{
	DECLARE_CLASS(UCodecMovieTheora,UCodecMovie,0,Engine)
};

#endif

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
