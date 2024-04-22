/*=============================================================================
	FullscreenMovie.h: Fullscreen movie playback support
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef _FULLSCREENMOVIE_H_
#define _FULLSCREENMOVIE_H_

/**
 * Movie loading flags
 */
enum EMovieFlags
{
	// streamed directly from disk
	MF_TypeStreamed  = 0x00,
	// preloaded from disk to memory
	MF_TypePreloaded = 0x01,
	// supplied memory buffer
	MF_TypeSupplied  = 0x02,
	// mask for above types
	MF_TypeMask      = 0x03,

	MF_LoopPlayback  = 0x80,

	/** Allows the user to PAUSE and UNPAUSE the movie by pressing START */
	MF_AllowUserToPause = 0x100,

	/** Set if user should only be able to skip the movie by pressing the BACK button.  Otherwise,
	    several possible buttons may be used to skip the movie */
	MF_OnlyBackButtonSkipsMovie = 0x200
};	


/**
* Movie playback modes
*/
enum EMovieMode
{
	// stream and loop from disk, minimal memory usage
	MM_LoopFromStream				= MF_TypeStreamed | MF_LoopPlayback,
	// stream from disk, minimal memory usage
	MM_PlayOnceFromStream			= MF_TypeStreamed,
	// load movie into RAM and loop from there
	MM_LoopFromMemory				= MF_TypePreloaded | MF_LoopPlayback,
	// load movie into RAM and play once
	MM_PlayOnceFromMemory			= MF_TypePreloaded,
	// loop from previously loaded buffer
	MM_LoopFromSuppliedMemory		= MF_TypeSupplied | MF_LoopPlayback,
	// play once from previously loaded buffer
	MM_PlayOnceFromSuppliedMemory	= MF_TypeSupplied,

	MM_Uninitialized				= 0xFFFFFFFF,
};


/**
* Abstract base class for full-screen movie player
*/
class FFullScreenMovieSupport : public FTickableObjectRenderThread, public FViewportClient
{
public:

	/**
	* Constructor
	*/
	FFullScreenMovieSupport()
	{}

	/** 
	* Destructor
	*/
	virtual ~FFullScreenMovieSupport() {};

	// FTickableObject interface
	
	/**
	 * Used to determine if a rendering thread tickable object must have rendering in a non-suspended
	 * state during it's Tick function.
	 *
	 * @return TRUE if the RHIResumeRendering should be called before tick if rendering has been suspended
	 */
	virtual UBOOL NeedsRenderingResumedForRenderingThreadTick() const
	{
		return TRUE;
	}

	// FFullScreenMovieSupport interface

	/**
	 * Kick off a movie play from the game thread
	 *
	 * @param InMovieMode How to play the movie (usually MM_PlayOnceFromStream or MM_LoopFromMemory).
	 * @param MovieFilename Path of the movie to play in its entirety
	 * @param StartFrame Optional frame number to start on
	 * @param InStartOfRenderingMovieFrame
	 * @param InEndOfRenderingMovieFrame
	 */
	virtual void GameThreadPlayMovie(EMovieMode InMovieMode, const TCHAR* MovieFilename, INT StartFrame=0, INT InStartOfRenderingMovieFrame=-1, INT InEndOfRenderingMovieFrame=-1) =0;

	/**
	* Stops the currently playing movie
	*
	* @param DelayInSeconds Will delay the stopping of the movie for this many seconds. If zero, this function will wait until the movie stops before returning.
	* @param bWaitForMovie if TRUE then wait until the movie finish event triggers
	* @param bForceStop if TRUE then non-skippable movies and startup movies are forced to stop
	*/
	virtual void GameThreadStopMovie(FLOAT DelayInSeconds=0.0f,UBOOL bWaitForMovie=TRUE,UBOOL bForceStop=FALSE) = 0;

	/**
	* Block game thread until movie is complete (must have been started
	* with GameThreadPlayMovie or it may never return)
	*/
	virtual void GameThreadWaitForMovie() = 0;

	/**
	* Checks to see if the movie has finished playing. Will return immediately
	*
	* @param MovieFilename MovieFilename to check against (should match the name passed to GameThreadPlayMovie). Empty string will match any movie
	* 
	* @return TRUE if the named movie has finished playing
	*/
	virtual UBOOL GameThreadIsMovieFinished(const TCHAR* MovieFilename) = 0;

	/**
	* Checks to see if the movie is playing. Will return immediately
	*
	* @param MovieFilename MovieFilename to check against (should match the name passed to GameThreadPlayMovie). Empty string will match any movie
	* 
	* @return TRUE if the named movie is playing
	*/
	virtual UBOOL GameThreadIsMoviePlaying(const TCHAR* MovieFilename) = 0;

	/**
	* Get the name of the most recent movie played
	*
	* @return Name of the movie that was most recently played, or empty string if a movie hadn't been played
	*/
	virtual FString GameThreadGetLastMovieName() = 0;

	/**
	* Kicks off a thread to control the startup movie sequence
	*/
	virtual void GameThreadInitiateStartupSequence() = 0;

	/**
	* Returns the current frame number of the movie (not thred synchronized in anyway, but it's okay 
	* if it's a little off
	*/
	virtual INT GameThreadGetCurrentFrame() = 0;

	/**
	 * Tells the movie player to allow game rendering for a frame while still keeping the movie
	 * playing. This will cover delays from render caching the first frame
	 */
	virtual void GameThreadRequestDelayedStopMovie() = 0;

	/**
	 * Controls whether the movie player processes input.
	 *
	 * @param	bShouldMovieProcessInput	whether the movie should process input.
	 */
	virtual void GameThreadToggleInputProcessing( UBOOL bShouldMovieProcessInput ) = 0;

	/**
	 * Controls whether the movie  is hidden and if input will forcibly stop the movie from playing when hidden/
	 *
	 * @param	bHidden	whether the movie should be hidden
	 */
	virtual void GameThreadSetMovieHidden( UBOOL bInHidden ) = 0;

	/**
	 * Mute / Unmute sound - for ex. when we are out of focus
	 */
	virtual void Mute( UBOOL InMute )
	{}

	/**
	 * Removes all overlays from displaying
	 */
	virtual void GameThreadRemoveAllOverlays()
	{}

	/**
	 * Adds a text overlay to the movie
	 *
	 * @param Font Font to use to display (must be in the root set so this will work during loads)
	 * @param Text Text to display
	 * @param X X location in resolution-independent coordinates (ignored if centered)
	 * @param Y Y location in resolution-independent coordinates
	 * @param ScaleX Text horizontal scale
	 * @param ScaleY Text vertical scale
	 * @param bIsCentered TRUE if the text should be centered
	 * @param bIsWrapped TRUE if the text should be wrapped at WrapWidth
	 * @param WrapWidth Number of pixels before text should wrap
	 */
	virtual void GameThreadAddOverlay(UFont* Font, const FString& Text, FLOAT X, FLOAT Y, FLOAT ScaleX, FLOAT ScaleY, UBOOL bIsCentered, UBOOL bIsWrapped, FLOAT WrapWidth )
	{}

	/**
	 * Fullscreen movies does not need hitproxy storage. Just a waste of memory.
	 */
	virtual UBOOL RequiresHitProxyStorage() { return FALSE; }
};

/*-----------------------------------------------------------------------------
Extern vars
-----------------------------------------------------------------------------*/

/** global for full screen movie player - see appInitFullScreenMoviePlayer */
extern FFullScreenMovieSupport* GFullScreenMovie;

#endif //_FULLSCREENMOVIE_H_
