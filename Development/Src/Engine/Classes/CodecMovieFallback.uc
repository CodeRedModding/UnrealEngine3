/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class CodecMovieFallback extends CodecMovie
	native;

/** seconds since start of playback */
var const transient float CurrentTime;

cpptext
{
	// CodecMovie interface

	/**
	* Not all codec implementations are available
	* @return TRUE if the current codec is supported
	*/
	virtual UBOOL IsSupported();
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
	 * Returns the movie format.
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
	* Resets the stream to its initial state so it can be played again from the beginning.
	*/
	virtual void ResetStream();
	/**
	* Queues the request to retrieve the next frame.
	*
	* @param InTextureMovieResource - output from movie decoding is written to this resource
	*/
	virtual void GetFrame( class FTextureMovieResource* InTextureMovieResource );
}
