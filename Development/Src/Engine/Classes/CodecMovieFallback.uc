class CodecMovieFallback extends CodecMovie
	native;

var const float	CurrentTime;

cpptext
{
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
	 * Tears down stream.
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
}