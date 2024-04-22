/*=============================================================================
	FullScreenMovieBink.h: Fullscreen movie playback support using Bink codec
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef _FULLSCREENMOVIEBINK_H_
#define _FULLSCREENMOVIEBINK_H_

#include "BinkHeaders.h"

// control if we should use background bink decoding
// disable Xbox as it's not working, although unclear why at the moment
#define USE_ASYNC_BINK (PS3 && USE_BINK_SPU) || XBOX

#if USE_ASYNC_BINK
extern BYTE GAsyncBinkThread1;
extern BYTE GAsyncBinkThread2;
#endif


/** Helper struct to hold information about a movie overlay string */
struct FFullScreenMovieOverlay
{
	/** Font for the string, for safety, this must be in the root set */
	UFont* Font;

	/** Text to display */
	FString Text;

	/** Location (resolution-independent coordinates, 0.0 - 1.0) */
	FLOAT X, Y;

	/** Scale */
	FLOAT ScaleX, ScaleY;

	/** TRUE if the text should be centered (X will be ignored) */
	UBOOL bIsCentered;

	/** TRUE if the text should be wrapped before it reaches WrapWidth.  Doesn't work with centered text.  */
	UBOOL bIsWrapped;

	/** When bIsWrapped is TRUE, the text will be wrapped to the next line when it reaches this width */
	FLOAT WrapWidth;
};

/**
 * Bink full-screen movie player
 */
class FFullScreenMovieBink : public FFullScreenMovieSupport
{
public:

	/** 
	 * Perform one-time initialization and create instance
	 *
	 * @param bUseSound - TRUE if sound should be enabled for movie playback
	 * @return new instance if successful
	 */
	static FFullScreenMovieSupport* StaticInitialize(UBOOL bUseSound);

	/**
	 * Destructor
	 */
	virtual ~FFullScreenMovieBink();

	// FTickableObject interface

	/**
	 * Pure virtual that must be overloaded by the inheriting class. It will
	 * be called from within UnLevTick.cpp after ticking all actors.
	 *
	 * @param DeltaTime	Game time passed since the last call.
	 */
	virtual void Tick(FLOAT DeltaTime);

	/**
	 * Pure virtual that must be overloaded by the inheriting class. It is
	 * used to determine whether an object is ready to be ticked. This is 
	 * required for example for all UObject derived classes as they might be
	 * loaded async and therefore won't be ready immediately.
	 *
	 * @return	TRUE if class is ready to be ticked, FALSE otherwise.
	 */
	virtual UBOOL IsTickable() const;

	// FViewportClient interface

	/**
	 * Check a key event received by the viewport.
	 * If the viewport client uses the event, it should return true to consume it.
	 * @param	Viewport - The viewport which the key event is from.
	 * @param	ControllerId - The controller which the key event is from.
	 * @param	Key - The name of the key which an event occured for.
	 * @param	Event - The type of event which occured.
	 * @param	AmountDepressed - For analog keys, the depression percent.
	 * @param	bGamepad - input came from gamepad (ie xbox controller)
	 * @return	True to consume the key event, false to pass it on.
	 */
	virtual UBOOL InputKey(FViewport* Viewport,INT ControllerId,FName Key,EInputEvent Event,FLOAT AmountDepressed,UBOOL bGamepad);

	/** 
	 * Process viewport close request
	 *
	 * @param Viewport - the viewprot that is being closed
	 */
	virtual void CloseRequested(FViewport* Viewport);

	/**
	 * Determine if the viewport client is going to need any keyboard input
	 * @return TRUE if keyboard input is needed
	 */
	virtual UBOOL RequiresKeyboardInput() const;

	// FFullScreenMovie interface

	/**
	 * Locate the requested movie accounting for language and loc versions in DLC
	 */
	UBOOL LocateMovieInDLC( UDownloadableContentManager* DlcManager, const TCHAR* RootMovieName );

	/**
	 * Locate the requested movie accounting for language and loc versions
	 */
	UBOOL LocateMovie( UDownloadableContentManager* DlcManager, const TCHAR* InMovieFilename );

	/**
	 * Kick off a movie play from the game thread
	 *
	 * @param InMovieMode How to play the movie (usually MM_PlayOnceFromStream or MM_LoopFromMemory).
	 * @param MovieFilename Path of the movie to play in its entirety
	 * @param StartFrame Optional frame number to start on
	 * @param InStartOfRenderingMovieFrame
	 * @param InEndOfRenderingMovieFrame
	 */
	virtual void GameThreadPlayMovie(EMovieMode InMovieMode, const TCHAR* MovieFilename, INT StartFrame=0, INT InStartOfRenderingMovieFrame=-1, INT InEndOfRenderingMovieFrame=-1);

	/**
	 * Stops the currently playing movie
	 *
	 * @param DelayInSeconds Will delay the stopping of the movie for this many seconds. If zero, this function will wait until the movie stops before returning.
	 * @param bWaitForMovie if TRUE then wait until the movie finish event triggers
	 * @param bForceStop if TRUE then non-skippable movies and startup movies are forced to stop
	 */
	virtual void GameThreadStopMovie(FLOAT DelayInSeconds=0.0f,UBOOL bWaitForMovie=TRUE,UBOOL bForceStop=FALSE);

	/**
	 * Block game thread until movie is complete (must have been started
	 * with GameThreadPlayMovie or it may never return)
	 */
	virtual void GameThreadWaitForMovie();

	/**
	 * Checks to see if the movie has finished playing. Will return immediately
	 *
	 * @param MovieFilename MovieFilename to check against (should match the name passed to GameThreadPlayMovie). Empty string will match any movie
	 * 
	 * @return TRUE if the named movie has finished playing
	 */
	virtual UBOOL GameThreadIsMovieFinished(const TCHAR* MovieFilename);

	/**
	 * Checks to see if the movie is playing. Will return immediately
	 *
	 * @param MovieFilename MovieFilename to check against (should match the name passed to GameThreadPlayMovie). Empty string will match any movie
	 * 
	 * @return TRUE if the named movie is playing
	 */
	virtual UBOOL GameThreadIsMoviePlaying(const TCHAR* MovieFilename);

	/**
	 * Get the name of the most recent movie played
	 *
	 * @return Name of the movie that was most recently played, or empty string if a movie hadn't been played
	 */
	virtual FString GameThreadGetLastMovieName();

	/**
	 * Kicks off a thread to control the startup movie sequence
	 */
	virtual void GameThreadInitiateStartupSequence();

	/**
	 * Returns the current frame number of the movie (not thred synchronized in anyway, but it's okay 
	 * if it's a little off
	 */
	virtual INT GameThreadGetCurrentFrame();

	/**
	 * Tells the movie player to allow game rendering for a frame while still keeping the movie
	 * playing. This will cover delays from render caching the first frame
	 */
	virtual void GameThreadRequestDelayedStopMovie();

	/**
	 * Controls whether the movie player processes input.
	 *
	 * @param	bShouldMovieProcessInput	whether the movie should process input.
	 */
	virtual void GameThreadToggleInputProcessing( UBOOL bShouldMovieProcessInput );

	/**
	 * Controls whether the movie  is hidden and if input will forcibly stop the movie from playing when hidden/
	 *
	 * @param	bHidden	whether the movie should be hidden
	 */
	virtual void GameThreadSetMovieHidden( UBOOL bInHidden );


	/**
	 * Mute / Unmute sound - for ex. when we are out of focus
	 */
	virtual void Mute( UBOOL InMute );

	/**
	 * Removes all overlays from displaying
	 */
	virtual void GameThreadRemoveAllOverlays();

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
	virtual void GameThreadAddOverlay(UFont* Font, const FString& Text, FLOAT X, FLOAT Y, FLOAT ScaleX, FLOAT ScaleY, UBOOL bIsCentered, UBOOL bIsWrapped, FLOAT WrapWidth );

protected:

	/** 
	 * Constructor
	 */
	FFullScreenMovieBink(UBOOL bUseSound=TRUE);

private:

	/**
	 * If necessary endian swaps the memory representing the Bink movie
	 *
	 * @param Buffer Memory that contains the Bink movie
	 * @param BufferSize Size of the memory
	 *
	 */
	void EnsureMovieEndianess(void* Buffer, DWORD BufferSize);

	/**
	 * Opens a Bink movie with streaming from disk
	 *
	 * @param MovieFilename Path to the movie file that will be played
	 *
	 * @return TRUE if the movie was initialized properly, FALSE otherwise
	 */
	UBOOL OpenStreamedMovie(const TCHAR* MovieFilename);

	/**
	 * Loads the entire Bink movie into RAM and opens it for playback
	 *
	 * @param MovieFilename		Path to the movie file that will be played
	 * @param Buffer			If NULL, opens movie from path, otherwise uses passed in memory
	 *
	 * @return TRUE if the movie was initialized properly, FALSE otherwise
	 */
	UBOOL OpenPreloadedMovie(const TCHAR* MovieFilename,void* Buffer);

	/**
	 * Opens a Bink movie from the supplied memory.
	 *
	 * @param Buffer Memory that contains the Bink movie
	 * @param BufferSize Size of the memory
	 *
	 * @return TRUE if the movie was initialized properly, FALSE otherwise
	 */
	UBOOL OpenSuppliedMovie(void* Buffer, DWORD BufferSize);

	/**
	 * Kicks off the playback of a movie
	 * 
	 * @param InMovieMode Determines the mode to play the movie
	 * @param MovieFilename Path to the movie file that will be played (or used to wait for if playing from supplied memory)
	 * @param Buffer Optional previously loaded buffer to a movie if using a SuppliedMemory mode
	 * @param BufferSize Size of Buffer
	 * @param StartFrame Optional frame to start on
	 * @param bIsSkippable Optional flag to specify if the movie can be skipped with X
	 *
	 * @return TRUE if successful
	 */
	UBOOL PlayMovie(EMovieMode InMovieMode, const TCHAR* MoviePath, const TCHAR* MovieFilename, void* Buffer=NULL, DWORD BufferSize=0, INT StartFrame=0, INT bIsSkippable=FALSE, INT InStartOfRenderingMovieFrame=-1, INT InEndOfRenderingMovieFrame=-1);

	/**
	 * Stops the movie and uninitializes the system. A new movie cannot be
	 * played until PrepareForMoviePlayback is called again.
	 *
	 * @param bForce	Whether we should force the movie to stop, even if it's not the last one in the sequence
	 * @return TRUE if successful
	 */
	UBOOL StopMovie( UBOOL bForce=FALSE );

	/**
	 * Stops the movie if it is skippable
	 */
	void SkipMovie();

	/** Queue a 'toggle pause' request which will be processed the next tick */
	void QueueUserTogglePauseRequest();

	/**
	 * Read next bink frame to texture and handle IO failure
	 */
	void BinkDecodeFrame( BINK* Bink );

	/**
	 * Perform per-frame processing, including rendering to the screen
	 *
	 * @return TRUE if movie is still playing, FALSE if movie ended or an error stopped the movie
	 */
	UBOOL PumpMovie();

	/**
	 * Stop current movie and proceed to the next startup movie (cleaning up any memory it can)
	 *
	 * @param bPlayNext	Whether we should proceed to the next startup movie
	 */
	void StopCurrentAndPlayNext( UBOOL bPlayNext=TRUE );

	/**
	 * Process the next step in the startup sequence.
	 *
	 * @return TRUE if a new movie was kicked off, FALSE otherwise
	 */
	UBOOL ProcessNextStartupSequence();

	/**
	 * Controls the background caching, keeping background caching paused while playing a Bink movie.
	 * If the movie is streaming from the DVD, we don't want unnecessary seeks.
	 * If the movie is playing from memory, we're normally loading something and still don't want unnecessary seeks.
	 */
	void ControlBackgroundCaching( UBOOL bEnable );

	/**
	 * Turns off game streaming and enabled movie streaming
	 */
	void SuspendGameIO();

	/**
	 * Turns on game streaming and disables movie streaming
	 */
	void ResumeGameIO();

	/**
	* Free any pre-allocated memory for movies in the startup list
	*/
	void FreeStartupMovieMemory();

	/** Synchronization object for game to wait for movie to finish */
	FEvent* MovieFinishEvent;

	/** An event used to block the game thread while the rendeing thread is reading a play-from-ram movie from disk */
	FEvent* MemoryReadEvent;

	/** The read in data for memory playback */
	TArray<BYTE> MemoryMovieBytes;

	/** Game thread's copy of the movie name */
	FString GameThreadMovieName;

	/** Movie thread's copy of the movie root path */
	FString MoviePath;

	/** Movie thread's copy of the movie name */
	FString MovieName;

	/** Is this a loop from memory movie? */
	EMovieMode MovieMode;

	/** Delay counter for delayed movie stop */
	FLOAT MovieStopDelay;

	/** Frame information */
	INT CurrentFrame, NumFrames;
	DOUBLE FrameRate;

	/** Bink handle */
	BINK* Bink;

	/** Whether we have paused background caching to HDD. */
	UBOOL bHasPausedBackgroundCaching;

	/** Tracks whether the file system should be used by Bink or by the game */
	UBOOL GameIOSuspended;

	/** TRUE if the movie can be skipped with a button press */
	UBOOL bIsMovieSkippable;

	/** TRUE if we manually started the rendering thread and it needs to be stopped */
	UBOOL bRenderingThreadStarted;

	/** used to save/restore the global flag */
	UBOOL SavedGUseThreadedRendering;

	/** used to mute/unmute bink audio when we are out of focus */
	UBOOL bUpdateAudio;
	UBOOL bMute;

	/**
	 * This will hide the rendering of the movie and NOT stop the movie based on input when this is true
	 * @see InputKey for not ending the movie based on input
	 * @see PumpMovie for where this var is checked to not update the render thread of the movie
	**/
	UBOOL bHideRenderingOfMovie;

	/**  When the fading in from just audio to audio and video should occur **/
	INT StartOfRenderingMovieFrame;

	/** When the fading from audio and video to just audio should occur **/
	INT EndOfRenderingMovieFrame;
	
	/** whether the movie can process input */
	UBOOL bEnableInputProcessing;

#if PS3
	/** TRUE if the movie is playing from BD; affects how errors are handled */
	UBOOL bIsPlayingFromBD;
#endif

	/** Buffer to store information about the startup movie sequence */
	class FStartupMovie
	{
	public:
		/** 
		* Constructor 
		*/
		FStartupMovie(const FString& InMovieName, UBOOL bInNeverFreeBuffer);		
		/** 
		* Destructor
		*/
		~FStartupMovie();		
		/** Root path of the movie */
		const FString MoviePath;
		/** Name of the movie */
		const FString MovieName;
		/** Movie bytes loaded from disk. Stays resident until movie finishes playing */
		void* Buffer;
		/** Size of the movie buffer */
		DWORD BufferSize;
		/** Used to signal when asynch loading of movie buffer has finished */
		FThreadSafeCounter LoadCounter;
		/** if TRUE then the movie buffer stays in memory */
		UBOOL bNeverFreeBuffer;
	};
	/** List of movies which are loaded and played at startup */
	TArray<FStartupMovie> StartupMovies;	
	/** Where are we in the startup sequence (-1 means normal playback) */
	INT StartupSequenceStep;	
	/** Where are we in the startup sequence (-1 means normal playback) */
	UBOOL bIsWaitingForEndOfRequiredMovies;

	/** list of startup movie names. from config */
	TArray<FString> StartupMovieNames;
	/** list of movie names that stay resident. from config */
	TArray<FString> AlwaysLoadedMovieNames;
	/** list of skippable movie names. from config */
	TArray<FString> SkippableMovieNames;

	/** Bink rendering */
	class FBinkMovieRenderClient* BinkRender;
	/** Bink audio */
	class FBinkMovieAudio* BinkAudio;
	/** Manages subtitle keys and text for the movies */
	class FSubtitleStorage* SubtitleStorage;
	/** The default read buffer size for Bink streaming movies */
	INT BinkIOStreamReadBufferSize;

	/** True if we've kicked off a movie on the game thread and BinkRender will need to be cleaned up later */
	UBOOL bGameThreadIsBinkRenderValid;


	/** List of all text overlays */
	TArray<FFullScreenMovieOverlay> TextOverlays;

	/** TRUE if user wants to toggle pause */
	UBOOL bUserWantsToTogglePause;

	/** TRUE if the user initiated a pause and we're currently displaying a still image */
	UBOOL bIsPausedByUser;

};

/**
* Audio support for Bink movies
*/
class FBinkMovieAudio
{
public:
	/**
	 * Constructor
	 */
	FBinkMovieAudio();

	/**
	 * Updates volume levels for movie voice and effects tracks (called before playing movie)
	 */
	void UpdateVolumeLevels();

	/** 
	 * Saves current volumes for all sound groups and sets volume to 0
	 */
	void PushSilence();

	/** 
	 * Restores previous volumes for all sound groups
	 */
	void PopSilence();

	/**
	 * Mute movie
	 */
	void Mute();

	/**
	 * Setup the bink audio tracks which should be loaded
	 * Should be called before opening bink movie for playback
	 */
	void SetSoundTracks( const TCHAR* MovieName );

	/**
	 * Routes audio tracks to appropriate channels
	 * Should be called after opening bink movie for playback
	 *
	 * @param Bink Handle to Bink interface
	 */
	void SetAudioChannels(BINK* Bink);

	/** 
	 * Handle setting of channel volumes for the XAudio2 system
	 */
	void HandleXAudio2Volumes(BINK* Bink);

	/** 
	 * Handle setting of channel volumes for the MultiStream system
	 */
	void HandleMultiStreamVolumes(BINK* Bink);

	/** 
	 * Handle setting of channel volumes for the DirectSound system
	 */
	void HandleDirectSoundVolumes(BINK* Bink);

private:
	/** The volume to apply to foley tracks (everything but center)  */
	FLOAT FoleyVolume;

	/** The volume to apply to voice tracks (center channel)  */
	FLOAT VoiceVolume;

	/** Saved master sound volume to restore after movie is done */
	FLOAT SavedMasterVolume;

	/** The speaker routings for each track */
	UINT BinkSoundTracks[8];
};

/**
 * Rendering support for Bink movies
 */
class FBinkMovieRenderClient
{
public:

	/** 
	 * Constructor
	 */
	FBinkMovieRenderClient(FViewportClient* InViewportClient);

	/** 
	 * Destructor
	 */
	virtual ~FBinkMovieRenderClient();

	/** 
	 * Create the viewport for rendering the movies to
	 */
	void CreateViewport();

	/** 
	 * Destroy the viewport that was created
	 */
	void DestroyViewport();

	/**
	 * Initialize internal rendering structures for a particular movie
	 *
	 * @param Bink Handle to Bink interface
	 * @return TRUE if successful
	 */
	UBOOL MovieInitRendering(BINK* Bink);

	/**
	 * Teardown internal rendering structures for a particular movie
	 */
	void MovieCleanupRendering();

	/**
	 * Start updating textures for a single movie frame
	 *
	 * @param Bink Handle to Bink interface
	 */
	void BeginUpdateTextures(BINK* Bink);

	/**
	 * End updating textures for a single movie frame
	 *
	 * @param Bink Handle to Bink interface
	 */
	void EndUpdateTextures(BINK* Bink);

	/**
	 * Start rendering for a single movie frame
	 */
	void BeginRenderFrame();
	
	/**
	 * End rendering for a single movie frame
	 */
	void EndRenderFrame();

	/** 
	 * Renders a word wrapped string to overlay the canvas
	 */
	void DisplayWrappedString( FCanvas* Canvas, const TCHAR* Text, UBOOL bCentered, UBOOL bTop, UFont* Font, FIntRect& Parms, const FLinearColor DrawColor );

	/**
	 * Render the latest decoded frame of the movie
	 *
	 * @param Bink Handle to Bink interface
	 * @param Subtitle The subtitle string to render this frame (can be NULL or empty)
	 * @param bLoadingMovie TRUE if we are rendering the loading movie
	 * @param TextOverlays List of overlays to draw in a addition to any subtitles
	 * @param bTriovizRenderToSceneColor TRUE if the movie should be rendered to a Trioviz scene color proxy
	 */
	void RenderFrame(BINK* Bink, const TCHAR* Subtitle, const TCHAR* InPauseText, UBOOL bLoadingMovie, const TArray<FFullScreenMovieOverlay>& TextOverlays, UBOOL bTriovizRenderToSceneColor=FALSE);

	/**
	 * Clear the screen
	 *
	 * @param ClearColor Color to clear with
	 */
	void RenderClear(const FLinearColor& ClearColor);

	/**
	 * Tick the viewport iot to handle input processing
	 *
	 * @param DeltaTime	Game time passed since the last call.
	 */
	void Tick(FLOAT DeltaTime);

	// accessors

	FViewport* GetViewport()
	{
		return Viewport;
	}

#if DWTRIOVIZSDK
	UBOOL DwTrioviz_HasAlpha(void)
	{
#if PS3
		return (BinkTextures.TextureSet != NULL && 
			BinkTextures.TextureSet->textures != NULL && 
			BinkTextures.TextureSet->textures->Atexture.offset != 0);
#else
		return (BinkTextures.TextureSet != NULL && 
			BinkTextures.TextureSet->textures != NULL && 
			BinkTextures.TextureSet->textures->Atexture.Num() != 0);
#endif
	}
#endif


protected:

	/**
	 * Computes a vertical scale and offset value for image aspect compensation
	 *
	 *
	 * @param ImageWidth Width of image
	 * @param ImageHeight Height of image
	 * @param CanvasWidth Width of canvas
	 * @param CanvasHeight Height of canvas
	 * @param AspectOffset [Out] Pixels to offset to compensate for aspect
	 * @param AspectScale [Out] Pixels to scale text to compensate for aspect
	 */
	void ComputeAspectOffsetAndScale( FLOAT ImageWidth, FLOAT ImageHeight, FLOAT CanvasWidth, FLOAT CanvasHeight, FVector2D& OutAspectOffset, FVector2D& OutAspectTextScale );


private:
	/** Have we found a width and height from the movie and initialized rendering buffers */
	UBOOL bInitializedMovieRendering;
	/** Viewport used to render */
	FViewport* Viewport;
	/** TRUE if we have a widescreen display */
	UBOOL bIsWideScreen;
	/** Viewport client for input/event handling */
	FViewportClient* ViewportClient;

	/** 
	 * Creates the internal bink frame buffer textures
	 */
	struct FInternalBinkTextures
	{
		FInternalBinkTextures() : bInitialized(FALSE) {}
		void Init(HBINK Bink);
		void Clear();

		/** internal frame buffers created for bink decoding. These are also used to render to the final surface */
		FBinkTextureSet* TextureSet;

		/** TRUE if internal bink frame buffer textures have been initialized */
		UBOOL bInitialized;
	};
	FInternalBinkTextures BinkTextures;

	/**
	 * Creates the shaders needed to copy the YUV texture results to a target
	 */
	struct FInternalBinkShaders
	{
		FInternalBinkShaders() : bInitialized(FALSE) {}
		~FInternalBinkShaders() { Clear(); }
		void Init();
		void Clear();

		/** TRUE if the internal bink shaders have been initialized. */
		UBOOL bInitialized;
	};
	static FInternalBinkShaders BinkShaders;

};


#endif //_FULLSCREENMOVIEBINK_H_

