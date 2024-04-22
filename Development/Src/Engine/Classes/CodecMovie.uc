/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class CodecMovie extends Object
	abstract
	transient
	native;

/** Cached script accessible playback duration of movie. */
var	const transient	float	PlaybackDuration;

cpptext
{
	// Can't have pure virtual functions in classes declared in *Classes.h due to DECLARE_CLASS macro being used.

	// CodecMovie interface

	/**
	* Not all codec implementations are available
	*
	* @return TRUE if the current codec is supported
	*/
	virtual UBOOL IsSupported() { return FALSE; }

	/**
	 * Returns the movie width.
	 *
	 * @return width of movie.
	 */
	virtual UINT GetSizeX() { return 0; }
	/**
	 * Returns the movie height.
	 *
	 * @return height of movie.
	 */
	virtual UINT GetSizeY()	{ return 0; }
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
	virtual FLOAT GetFrameRate() { return 0; }
	
	/**
	 * Initializes the decoder to stream from disk.
	 *
	 * @param	Filename	Filename of compressed media.
	 * @param	Offset		Offset into file to look for the beginning of the compressed data.
	 * @param	Size		Size of compressed data.
	 *
	 * @return	TRUE if initialization was successful, FALSE otherwise.
	 */
	virtual UBOOL Open( const FString& Filename, DWORD Offset, DWORD Size ) { return FALSE; }
	/**
	 * Initializes the decoder to stream from memory.
	 *
	 * @param	Source		Beginning of memory block holding compressed data.
	 * @param	Size		Size of memory block.
	 *
	 * @return	TRUE if initialization was successful, FALSE otherwise.
	 */
	virtual UBOOL Open( void* Source, DWORD Size ) { return FALSE; }	
	/**
	 * Tears down stream.
	 */	
	virtual void Close() {}

	/**
	 * Resets the stream to its initial state so it can be played again from the beginning.
	 */
	virtual void ResetStream() {}
	/**
	 * Queues the request to retrieve the next frame.
	 *
 	 * @param InTextureMovieResource - output from movie decoding is written to this resource
	 */
	virtual void GetFrame( class FTextureMovieResource* InTextureMovieResource ) {}
	/**
	 * Returns the playback time of the movie.
	 *
	 * @return playback duration of movie.
	 */
	virtual FLOAT GetDuration() { return PlaybackDuration; }
	/** 
	* Begin playback of the movie stream 
	*
	* @param bLooping - if TRUE then the movie loops back to the start when finished
	* @param bOneFrameOnly - if TRUE then the decoding is paused after the first frame is processed 
	* @param bResetOnLastFrame - if TRUE then the movie frame is set to 0 when playback finishes
	*/
	virtual void Play(UBOOL bLooping, UBOOL bOneFrameOnly, UBOOL bResetOnLastFrame) {}
	/** 
	* Pause or resume the movie playback.
	*
	* @param bPause - if TRUE then decoding will be paused otherwise it resumes
	*/
	virtual void Pause(UBOOL bPause) {}
	/**
	* Stop playback from the movie stream 
	*/ 
	virtual void Stop() {}
	
	/**
	* Release any dynamic rendering resources created by this codec
	*/
	virtual void ReleaseDynamicResources() {}
}
