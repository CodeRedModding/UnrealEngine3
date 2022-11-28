class CodecMovie extends Object
	abstract
	native;

/** Cached script accessible playback duration of movie. */
var	const transient	float	PlaybackDuration;

cpptext
{
	// Can't have pure virtual functions in classes declared in *Classes.h due to DECLARE_CLASS macro being used.

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
	virtual EPixelFormat GetFormat() { return PF_Unknown; }
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
	 * Blocks until the decoder has finished the pending decompression request.
	 *
	 * @return	Time into movie playback as seen from the decoder side.
	 */
	virtual FLOAT BlockUntilIdle() { return 0.f; }	
	/**
	 * Queues the request to retrieve the next frame.
	 *
 	 * @param	Destination		Memory block to uncompress data into.
	 * @return	FALSE if the end of the frame has been reached and the frame couldn't be completed, TRUE otherwise
	 */
	virtual UBOOL GetFrame( void* Destination ) { return FALSE; }

	/**
	 * Returns the playback time of the movie.
	 *
	 * @return playback duration of movie.
	 */
	virtual FLOAT GetDuration() { return PlaybackDuration; }
}