/*=============================================================================
	FullScreenMovieBink.inl: Fullscreen movie playback support using Bink codec
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#if USE_BINK_CODEC

#include "FullScreenMovieBink.h"
#include "EngineSoundClasses.h"
#include "EngineAudioDeviceClasses.h"
#include "UnSubtitleManager.h"
#include "SubtitleStorage.h"
#include "UnAudio.h"
#include "UnAudioEffect.h"
#include "Localization.h"

#if XBOX
#include "FFileManagerXenon.h"
#include "XeDrv.h"
#include "XAudio2Device.h"
#elif PS3
#include "FFileManagerPS3.h"
#include "PS3AudioDevice.h"
#include "PS3Threading.h"
#include "PS3Controller.h"
#include "PS3Client.h"
#include "PS3Viewport.h"
#else
#include "XAudio2Device.h"
#endif

#if DWTRIOVIZSDK
#include "DwTrioviz/DwTriovizImpl.h"
#include "SceneRenderTargets.h"

extern INT			GScreenWidth;
extern INT			GScreenHeight;


#undef BinkOpen
#undef BinkDoFrame
RADEXPFUNC HBINK RADEXPLINK BinkOpen(char const * name,U32 flags);
RADEXPFUNC S32  RADEXPLINK BinkDoFrame(HBINK bnk);

class FDwSceneRenderTargetSceneColorProxy : public FRenderTarget
{
public:


	/**
	* @return width of the scene render target this proxy will render to
	*/
	virtual UINT GetSizeX() const
	{
		return GScreenWidth;
	}

	/**
	* @return height of the scene render target this proxy will render to
	*/
	virtual UINT GetSizeY() const
	{
		return GScreenHeight;
	}

	/**
	* @return gamma this render target should be rendered with
	*/
	virtual FLOAT GetDisplayGamma() const
	{
		return 1.0f;
	}

	virtual const FSurfaceRHIRef& GetRenderTargetSurface() const
	{
#if XBOX
		return GSceneRenderTargets.GetSceneColorLDRSurface();
#else
		return GSceneRenderTargets.GetSceneColorSurface();
#endif
	}
};
#endif


/*-----------------------------------------------------------------------------
	FFullScreenMovieBink
-----------------------------------------------------------------------------*/

#if XBOX || PS3
	extern FViewport* GFullScreenMovieViewport;
#endif


/**
 * Wrapper function to force Bink allocations to go through the UE3 allocator 
 */
static void* RADLINK BinkMalloc(UINT Size)
{
	return appMalloc(Size,32);
}

/**
 * Wrapper function to force Bink deallocations to go through the UE3 allocator 
 */
static void RADLINK BinkFree(void* Ptr)
{
	appFree(Ptr);
}

/**
 * Strip off the ".bik" extension from filename.
 * e.g. GetFilenameWithoutBinkExtension("File.bik") -> "File"
 *      GetFilenameWithoutBinkExtension("File.Name") -> "File.Name"
 *
 * @param InPath 
 *
 */
static FString GetFilenameWithoutBinkExtension( const FString & InPath, bool bRemovePath)
{
	return InPath.EndsWith(TEXT(".bik")) ? FFilename(InPath).GetBaseFilename(bRemovePath) : InPath;
}

/**
 * Movie tweaking paramters for bink playback
 */
UBOOL GSuspendGameIODuringStreaming = TRUE;
INT GBinkFrameSkipCount = 0;

/** Remembers which thread IDs to use for async bink tasks */
BYTE GAsyncBinkThread1 = 0;
BYTE GAsyncBinkThread2 = 0;


/** 
 * Constructor
 */
FFullScreenMovieBink::FFullScreenMovieBink(UBOOL bUseSound)
:	MovieFinishEvent(NULL)
,	MemoryReadEvent(NULL)
,	GameThreadMovieName(TEXT(""))
,	MoviePath(TEXT(""))
,	MovieName(TEXT(""))
,	MovieMode(MM_Uninitialized)
,	MovieStopDelay(0.0f)
,	CurrentFrame(-1)
,	NumFrames(-1)
,	FrameRate(1.f/30.f)
,	Bink(NULL)
,	bHasPausedBackgroundCaching(FALSE)
,	GameIOSuspended(FALSE)
,	bIsMovieSkippable(FALSE)
,	bRenderingThreadStarted(FALSE)
,	SavedGUseThreadedRendering(GUseThreadedRendering)
,	bUpdateAudio(TRUE)
,	bMute(FALSE)
,	bHideRenderingOfMovie(FALSE)
,	bEnableInputProcessing(TRUE)
,	StartupSequenceStep(-1)
,	bIsWaitingForEndOfRequiredMovies(FALSE)
,	BinkRender(NULL) // deferred init 
,	BinkAudio(bUseSound ? new FBinkMovieAudio() : NULL)
,	SubtitleStorage(new FSubtitleStorage())
,	BinkIOStreamReadBufferSize(0)
,	bGameThreadIsBinkRenderValid( FALSE )
,	bUserWantsToTogglePause( FALSE )
,	bIsPausedByUser( FALSE )
{
	// create our synchronization objects (a manual reset Event)
	check(GSynchronizeFactory);
	MovieFinishEvent = GSynchronizeFactory->CreateSynchEvent(TRUE);
	MemoryReadEvent	 = GSynchronizeFactory->CreateSynchEvent(TRUE);

#if PS3
	// override bink allocators
	BinkSetMemory(BinkMalloc,BinkFree);

	// get the rendering thread priority
	extern INT GRenderThreadPriorityDuringStreaming;
	GConfig->GetInt(TEXT("Engine.StreamingMovies"), TEXT("RenderPriorityPS3"), GRenderThreadPriorityDuringStreaming, GEngineIni);
#endif


#if USE_ASYNC_BINK

	// set up background threads
	void* Initializer = NULL;

#if PS3

	// fill out the SPU initializer
	void* SpursInfo[2];
	SpursInfo[0] = GSPURS;
	extern char _binary_Bink_start[];
	SpursInfo[1] = _binary_Bink_start;

	// always use thread 0
	GAsyncBinkThread1 = GAsyncBinkThread2 = 0;

	// only use a second thread if running at 1080
	if (GScreenHeight >= 1080)
	{
		GAsyncBinkThread2 = 1;
	}

	// pass this to the thread creation
	Initializer = SpursInfo;

#elif XBOX

	// reuse the stats threads (they should be pretty good to use while playing movies)
	GAsyncBinkThread1 = GAsyncBinkThread2 = STATS_SENDER_HWTHREAD;
	if (GScreenHeight >= 1080)
	{
		GAsyncBinkThread2 = STATS_LISTENER_HWTHREAD;
	}

#else
	#error Startup Bink threads for your platform.
#endif

	// start the async thread(s)
	if (!BinkStartAsyncThread(GAsyncBinkThread1, Initializer))
	{
		appErrorf(TEXT("BinkStartAsyncThread(0) failed."));
	}

	// if we want a different thread for the second one, then make a second one
	if (GAsyncBinkThread1 != GAsyncBinkThread2)
	{
		if (!BinkStartAsyncThread(GAsyncBinkThread2, Initializer))
		{
			appErrorf(TEXT("BinkStartAsyncThread(1) failed."));
		}
	}

#endif

	// set up tweaking parameters
	GConfig->GetBool(TEXT("Engine.StreamingMovies"), TEXT("SuspendGameIO"), GSuspendGameIODuringStreaming, GEngineIni);

	// Set up the default Bink IO read buffer size
	GConfig->GetInt(TEXT("FullScreenMovie"), TEXT("BinkIOStreamReadBufferSize"), BinkIOStreamReadBufferSize, GEngineIni);

	// by default, we are "done" playing a movie
	MovieFinishEvent->Trigger();

// jmarshall
	StartupMovieNames.AddUniqueItem(TEXT("DukeIntro"));
// jmarshall end

	// read movie entries from config file
	FConfigSection* MovieIni = GConfig->GetSectionPrivate( TEXT("FullScreenMovie"), FALSE, TRUE, GEngineIni );
	if( MovieIni )
	{
		for( FConfigSectionMap::TIterator It(*MovieIni); It; ++It )
		{
			// add to list of startup movies
			if( It.Key() == TEXT("StartupMovies") )
			{
				StartupMovieNames.AddUniqueItem(*(It.Value()));
			}
			// add to list of memory resident movies
			else if( It.Key() == TEXT("AlwaysLoadedMovies") )
			{
				AlwaysLoadedMovieNames.AddUniqueItem(*(It.Value()));
			}
			// add to list of movies that are skippable
			else if( It.Key() == TEXT("SkippableMovies") )
			{
				SkippableMovieNames.AddUniqueItem(*(It.Value()));
			}
		}
	}
	// initialize list of startup movies
	for( INT Idx=0; Idx < StartupMovieNames.Num(); Idx++ )
	{
		const FString& TheMovieName = StartupMovieNames(Idx);
		const UBOOL bAlwaysLoaded = AlwaysLoadedMovieNames.FindItemIndex(TheMovieName) != INDEX_NONE;
		new(StartupMovies) FStartupMovie(TheMovieName,bAlwaysLoaded);		
	}
}

/**
 * Destructor
 */
FFullScreenMovieBink::~FFullScreenMovieBink()
{
#if USE_ASYNC_BINK
	BinkRequestStopAsyncThread(GAsyncBinkThread1);
	if (GAsyncBinkThread1 != GAsyncBinkThread2)
	{
		BinkRequestStopAsyncThread(GAsyncBinkThread2);
	}

	BinkWaitStopAsyncThread(GAsyncBinkThread1);
	if (GAsyncBinkThread1 != GAsyncBinkThread2)
	{
		BinkWaitStopAsyncThread(GAsyncBinkThread2);
	}
#endif

	bGameThreadIsBinkRenderValid = FALSE;
	delete BinkRender;
	delete BinkAudio;
	delete SubtitleStorage;
}

/** 
 * Perform one-time initialization and create instance
 *
 * @param bUseSound - TRUE if sound should be enabled for movie playback
 * @return new instance if successful
 */
FFullScreenMovieSupport* FFullScreenMovieBink::StaticInitialize(UBOOL bUseSound)
{
	static FFullScreenMovieBink* StaticInstance = NULL;
	if( !StaticInstance )
	{
		StaticInstance = new FFullScreenMovieBink(bUseSound);
	}
	return StaticInstance;
}

/**
 * Pure virtual that must be overloaded by the inheriting class. It will
 * be called from within UnLevTick.cpp after ticking all actors.
 *
 * @param DeltaTime	Game time passed since the last call.
 */
void FFullScreenMovieBink::Tick(FLOAT DeltaTime)
{
	// if we're delaying the movie stop, count down the delay
	if( MovieStopDelay != 0 )
	{
		MovieStopDelay -= DeltaTime;
		// if the delay ran out, then we will want to stop the movie
		if( MovieStopDelay <= 0.0f )
		{
			StopMovie();
		}
	}

	DOUBLE CurTime = appSeconds();
	// tick viewport for input processing
	check(BinkRender);
	BinkRender->Tick(DeltaTime);

	// we are only tickable if we are playing a movie
	if( Bink && 
		PumpMovie() )
	{
#if XBOX
		// yield some IO time if streaming movie from file
		if( (MovieMode & MF_TypeMask) == MF_TypeStreamed )
		{
			BINKREALTIME RealTime;
			BinkGetRealtime(Bink, &RealTime, 0);
			UINT PercentageFull = 100;
			if (RealTime.ReadBufferSize)
			{
				PercentageFull = (RealTime.ReadBufferUsed * 100) / RealTime.ReadBufferSize;
			}
			if (PercentageFull > 80)
			{
				ResumeGameIO();
			}
			else if (PercentageFull < 20)
			{
				SuspendGameIO();
			}
		}
#endif
	}

	DOUBLE TotalBinkTime = appSeconds() - CurTime;
	if (TotalBinkTime > 0.040)
	{
		debugf(NAME_DevMovie, TEXT("!!! BinkTick took too long: %.3f. Skipped %d frames."), FLOAT(TotalBinkTime), GBinkFrameSkipCount);
	}
}

/**
 * Pure virtual that must be overloaded by the inheriting class. It is
 * used to determine whether an object is ready to be ticked. This is 
 * required for example for all UObject derived classes as they might be
 * loaded async and therefore won't be ready immediately.
 *
 * @return	TRUE if class is ready to be ticked, FALSE otherwise.
 */
UBOOL FFullScreenMovieBink::IsTickable() const
{
	return( Bink != NULL && 
			BinkRender != NULL && 
			// make sure we are not ticking between scene rendering 
			!RHIIsDrawingViewport() );
}

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
UBOOL FFullScreenMovieBink::InputKey(FViewport* Viewport,INT ControllerId,FName Key,EInputEvent Event,FLOAT AmountDepressed,UBOOL bGamepad)
{
	UBOOL Result = FALSE;

	if( ( bEnableInputProcessing ) && ( bHideRenderingOfMovie == FALSE ) )
	{
		UBOOL bIsMoviePlaying = Bink && !MovieFinishEvent->Wait(0);
		if( Event == IE_Pressed &&
			bIsMoviePlaying )
		{

			// Start button pressed?
			if( Key == KEY_XboxTypeS_Start ) 
			{
				// Check to see if the user is allowed to PAUSE or UNPAUSE the movie
				const UBOOL bIsLoadingMovie = ( MovieMode & MF_LoopPlayback );
				if( !bIsLoadingMovie && ( MovieMode & MF_AllowUserToPause ) )
				{
					// User would like to toggle pause!  We'll queue the request and do that the next
					// time we update the movie.
					if( IsInGameThread() )
					{
						ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
							QueueBinkMovieTogglePauseCommand,
							FFullScreenMovieBink*, MoviePlayer, this,
						{
							MoviePlayer->QueueUserTogglePauseRequest();
						});
					}
					else
					{				
						QueueUserTogglePauseRequest();
					}

					Result = TRUE;
				}
			}

			if( !bIsPausedByUser )
			{
				// Check to see if the user wants to skip the movie
				if( ( Key == KEY_XboxTypeS_Back ||
					( Key == KEY_XboxTypeS_A && !( MovieMode & MF_OnlyBackButtonSkipsMovie ) ) ||
					( Key == KEY_XboxTypeS_Start && !( MovieMode & ( MF_OnlyBackButtonSkipsMovie | MF_AllowUserToPause ) ) ) ||
#if PS3
					// if the PS3 was set to use Circle for accept instead of X, then use that one here
					Key == (appPS3UseCircleToAccept() ? KEY_XboxTypeS_B : KEY_XboxTypeS_A) ||
#endif
					Key == KEY_LeftMouseButton ||
					Key == KEY_Escape ) &&
					GIsRunning )
				{
					if( IsInGameThread() )
					{
						ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
							SkipBinkMovieCommand,
							FFullScreenMovieBink*, MoviePlayer, this,
						{
							MoviePlayer->SkipMovie();
						});
					}
					else
					{				
						SkipMovie();
					}
					Result = TRUE;
				}
			}
		}
	}

	return Result;
}



/** Queue a 'toggle pause' request which will be processed the next tick */
void FFullScreenMovieBink::QueueUserTogglePauseRequest()
{
	bUserWantsToTogglePause = TRUE;
}


/** 
 * Process viewport close request
 *
 * @param Viewport - the viewprot that is being closed
 */
void FFullScreenMovieBink::CloseRequested(FViewport* Viewport)
{
	if( BinkRender &&
		BinkRender->GetViewport() &&
		BinkRender->GetViewport() == Viewport )
	{
		// force playback to end if our viewport is being closed
		GameThreadStopMovie(0,FALSE,TRUE);
	}
}

/**
 * Determine if the viewport client is going to need any keyboard input
 * @return TRUE if keyboard input is needed
 */
UBOOL FFullScreenMovieBink::RequiresKeyboardInput() const
{
#if CONSOLE
	// needed due to conflict with the keyboard input event handler on PS3
	// and it's not really needed on consoles anyway
	return FALSE;
#else
	return TRUE;
#endif
}

/**
 * Locate the requested movie accounting for language and loc versions in DLC
 */
UBOOL FFullScreenMovieBink::LocateMovieInDLC( UDownloadableContentManager* DlcManager, const TCHAR* RootMovieName )
{
	FName RootMovieNameName( RootMovieName );
	if( DlcManager->GetDLCNonPackageFilePath( RootMovieNameName, MoviePath ) )
	{
		INT GameOffset = MoviePath.InStr( FString( TEXT( "Movies" ) ), FALSE, TRUE );
		if( GameOffset >= 0 )
		{
			MoviePath = MoviePath.Left( GameOffset );
			return TRUE;
		}
	}

	return FALSE;
}

/**
 * Locate the requested movie accounting for language and loc versions
 */
UBOOL FFullScreenMovieBink::LocateMovie( UDownloadableContentManager* DlcManager, const TCHAR* RootMovieName )
{
	debugf( NAME_DevMovie, TEXT( "Attempting to find movie: %s" ), RootMovieName );

	// Check that the base movie file exists.
	MovieName = RootMovieName;

	// Checking in DLC
	if( DlcManager != NULL )
	{
		debugf( NAME_DevMovie, TEXT( " ... trying DLC." ) );
		if( LocateMovieInDLC( DlcManager, RootMovieName ) )
		{
			return TRUE;
		}
	}

	debugf( NAME_DevMovie, TEXT( " ... could not find movie; trying filesystem." ) );

	FString FullPath = MoviePath + TEXT( "Movies\\" ) + MovieName + TEXT( ".bik" );
	if( GFileManager->FileSize( *FullPath ) != -1 )
	{
		return TRUE;
	}

	// Check for language specific version
	FString LangExt( appGetLanguageExt() );
	if( LangExt != TEXT( "INT" ) )
	{
		FFilename Filename( *FullPath );
		FString LocFilename = Filename.GetLocalizedFilename( *LangExt );		
		MovieName = GetFilenameWithoutBinkExtension( LocFilename, true );

		// Checking in DLC
		if( DlcManager != NULL )
		{
			debugf( NAME_DevMovie, TEXT( " ... could not find movie; trying language specific movie in DLC." ) );
			if( LocateMovieInDLC( DlcManager, *LocFilename ) )
			{
				return TRUE;
			}
		}

		debugf( NAME_DevMovie, TEXT( " ... could not find movie; trying language specific movie on filesystem." ) );
		if( GFileManager->FileSize( *LocFilename ) != -1 )
		{
			return TRUE; 
		}
	}

	// Or the LOC equivalent exists
	MovieName = RootMovieName;
	MovieName += TEXT( "_LOC" );

	// Checking in DLC
	if( DlcManager != NULL )
	{
		debugf( NAME_DevMovie, TEXT( " ... could not find movie; trying LOC movie in DLC." ) );
		if( LocateMovieInDLC( DlcManager, *MovieName ) )
		{
			return TRUE;
		}
	}

	debugf( NAME_DevMovie, TEXT( " ... could not find movie; trying LOC movie on filesystem." ) );
	FullPath = MoviePath + TEXT( "Movies\\" ) + MovieName + TEXT( ".bik" );
	if( GFileManager->FileSize( *FullPath ) != -1 )
	{
		return TRUE;
	}

	return FALSE;
}

/**
 * Kick off a movie play from the game thread
 *
 * @param InMovieMode How to play the movie (usually MM_PlayOnceFromStream or MM_LoopFromMemory).
 * @param InMovieFilename Path of the movie to play in its entirety
 * @param InStartFrame Optional frame number to start on
 * @param InStartOfRenderingMovieFrame
 * @param InEndOfRenderingMovieFrame
 */
void FFullScreenMovieBink::GameThreadPlayMovie(EMovieMode InMovieMode, const TCHAR* InMovieFilename, INT InStartFrame, INT InStartOfRenderingMovieFrame, INT InEndOfRenderingMovieFrame)
{
#if !CONSOLE
	// If we're going to play startup movies, hide the splashscreen and show the game window.
	// Both of these functions do nothing if called a second time.
	if ( GIsGame )
	{
		extern void appHideSplash();
		extern void appShowGameWindow();
		appHideSplash();
		appShowGameWindow();
	}
#endif

#if PS3
	// make sure the ps3 is in a state where it can use the file system from the rendering thread
	extern UBOOL GNoRecovery;
	if (GNoRecovery)
	{
		return;
	}
#endif

	// Check for movie already playing and exit out if so
	if( MovieFinishEvent->Wait( 0 ) == FALSE )
	{
		debugf(NAME_DevMovie,  TEXT( "Attempting to start already playing movie \"%s\"; aborting" ), *GameThreadMovieName );
		return;
	}

	// Remember the undecorated name of the movie we're playing so kismet can use it without any modification
	FString MovieFilename(InMovieFilename);
	GameThreadMovieName = GetFilenameWithoutBinkExtension(MovieFilename, true);

	// Check to see if the movie exists
	MoviePath = appGameDir();
	UDownloadableContentManager* DlcManager = UGameEngine::GetDLCManager();
	if( !LocateMovie( DlcManager, *GameThreadMovieName ) )
	{
		debugf( NAME_DevMovie, TEXT( " ... could not find movie; aborting." ) );
		return;
	}

	FString FullPath = MoviePath + TEXT( "Movies\\" ) + MovieName + TEXT( ".bik" );
	debugf( NAME_DevMovie, TEXT( " ... found movie: %s" ), *FullPath );

	// deferred init for bink renderer the first time we try playing a movie
	if( !BinkRender )
	{
		FBinkMovieRenderClient* NewBinkRender = new FBinkMovieRenderClient(this);
		ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
			InitBinkRender,
			FBinkMovieRenderClient*, NewBinkRender, NewBinkRender,
			FFullScreenMovieBink*, MoviePlayer, this,
		{
			MoviePlayer->BinkRender = NewBinkRender;
		});

		// We've enqueued the binding of our newly-created BinkRender object, so make a note of that
		// on the game thread to avoid race conditions with stopping a movie immediately after
		// starting it.
		bGameThreadIsBinkRenderValid = TRUE;
	}

	if( !GIsThreadedRendering )
	{
		// start the rendering thread if it is not enabled
		SavedGUseThreadedRendering = GUseThreadedRendering;
		GUseThreadedRendering = TRUE;		
		StartRenderingThread();
		bRenderingThreadStarted = TRUE;
	}
	else
	{
		// make sure rendering thread is done rendering
		FlushRenderingCommands();
	}

	// make sure game isn't rendered while movie is playing
	FViewport::SetGameRenderingEnabled(FALSE);	

	// reset synchronization to untriggered state
	MovieFinishEvent->Reset();

	if( InMovieMode & MF_TypePreloaded )
	{
		debugf(NAME_DevMovie, TEXT("Starting memory preloaded movie...%s (%s)"), *GameThreadMovieName, *MovieName);
		// reset synch event to trigger loading of memory resident movie
		MemoryReadEvent->Reset();
	}
	else if( InMovieMode & MF_TypeSupplied )
	{
		debugf(NAME_DevMovie, TEXT("Starting memory supplied movie...%s (%s)"), *GameThreadMovieName, *MovieName);
	}
	else
	{
		debugf(NAME_DevMovie, TEXT("Starting file streamed movie...%s (%s)"), *GameThreadMovieName, *MovieName);
	}

	if( BinkAudio && !bMute )
	{
		// Check the movie volume levels and update if necessary
		BinkAudio->UpdateVolumeLevels();
	}

	// check for skippable movies. only allowed on servers
	UBOOL bIsSkippable = (SkippableMovieNames.FindItemIndex(GameThreadMovieName) != INDEX_NONE) && 
		(!GWorld || GWorld->GetWorldInfo()->NetMode != NM_Client);
	
	// queue playback parameters to render thread
	struct FPlayMovieParams
	{
		FString MoviePath;
		FString MovieName;
		EMovieMode MovieMode;
		INT StartFrame;
		UBOOL bIsSkippable;
		INT StartOfRenderingMovieFrame;
		INT EndOfRenderingMovieFrame;
	};

	FPlayMovieParams PlayMovieParams = 
	{
		MoviePath, 
		MovieName, 
		InMovieMode,
		InStartFrame,
		bIsSkippable,
		InStartOfRenderingMovieFrame,
		InEndOfRenderingMovieFrame
	};
	ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
		PlayGearsMovieCommand,
		FPlayMovieParams, Params, PlayMovieParams,
		FFullScreenMovieBink*, MoviePlayer, this,
	{
		// start playing the movie
		MoviePlayer->PlayMovie(
			Params.MovieMode, 
			*Params.MoviePath, 
			*Params.MovieName, 
			NULL,0,
			Params.StartFrame,
			Params.bIsSkippable,
			Params.StartOfRenderingMovieFrame,
			Params.EndOfRenderingMovieFrame
			);

		// make sure the movie gets a chance to start up, as the next tick may come later
		MoviePlayer->Tick(1.0f / 30.0f);
	});

	// wait for the memory to be read in for ram based movie (so game thread loading doesn't conflict with this loading)
	if( InMovieMode & MF_TypePreloaded && !GIsRequestingExit)
	{
		debugf(NAME_DevMovie, TEXT("Waiting for memory preloaded movie to finish loading ..."));
		MemoryReadEvent->Wait();
		debugf(NAME_DevMovie, TEXT("Got memory preloaded movie event!!!"));
	}
}

/**
 * Stops the currently playing movie
 *
 * @param DelayInSeconds Will delay the stopping of the movie for this many seconds. If zero, this function will wait until the movie stops before returning.
 * @param bWaitForMovie if TRUE then wait until the movie finish event triggers
 * @param bForceStop if TRUE then non-skippable movies and startup movies are forced to stop
 */
void FFullScreenMovieBink::GameThreadStopMovie(FLOAT DelayInSeconds,UBOOL bWaitForMovie,UBOOL bForceStop) 
{
	debugf(NAME_DevMovie, TEXT("Stopping movie %s"),*GameThreadMovieName);


	if( bGameThreadIsBinkRenderValid )
	{
		// If this is a looping movie (loading screen) then we'll flush all async loads before we
		// cancel the movie to give the texture streaming a chance to catch up.  This can be safely
		// removed if it's ever a problem; we're simply using the fact that a load screen is still
		// up to hide some of the texture streaming cost
		if( MovieMode & MF_LoopPlayback )
		{
			// Block for a while, streaming in all textures
			GStreamingManager->StreamAllResources( TRUE );
		}

		ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
			StopGearsMovieCommand,
			UBOOL, bForceStop, bForceStop,
			FFullScreenMovieBink*, MoviePlayer, this,
		{
			{
				MoviePlayer->StopMovie( bForceStop );
			}
		});	

		// wait for the movie to finish (to make sure required movies are played before continuing)
		if( bWaitForMovie )
		{
			GameThreadWaitForMovie();
		}

		// shutdown the rendering thread if it was manually started
		if( bRenderingThreadStarted )
		{
			StopRenderingThread();
			GUseThreadedRendering = SavedGUseThreadedRendering;			
			bRenderingThreadStarted = FALSE;

			// Delete the BinkRender (viewport) to free up the GPU memory.
			bGameThreadIsBinkRenderValid = FALSE;
			delete BinkRender;
			BinkRender = NULL;
		}
		else
		{
			// Delete the BinkRender (viewport) to free up the GPU memory.
			ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
				DeleteBinkRenderCommand,FBinkMovieRenderClient*,BinkRender,BinkRender,
			{
				delete BinkRender;
			});
			FlushRenderingCommands();
			bGameThreadIsBinkRenderValid = FALSE;
			BinkRender = NULL;
		}
	}
}

/**
 * Block game thread until movie is complete (must have been started
 * with GameThreadPlayMovie or it may never return)
 */
void FFullScreenMovieBink::GameThreadWaitForMovie()
{
	// wait for the event
	UBOOL bIsFinished = MovieFinishEvent->Wait( 1 );
	while ( !bIsFinished && !GIsRequestingExit )
	{
#if !CONSOLE && !PLATFORM_UNIX
		// Handle all incoming messages if we're not using wxWindows in which case this is done by their
		// message loop.
		extern UBOOL GUsewxWindows;
		if( !GUsewxWindows )
		{
			appWinPumpMessages();
		}

		// Compute the time since the last tick.
		static DOUBLE LastTickTime = appSeconds();
		const DOUBLE CurrentTime = appSeconds();
		const FLOAT DeltaTime = CurrentTime - LastTickTime;
		LastTickTime = CurrentTime;

		// Allow the client to process deferred window messages and input.
		check(GEngine->Client);
		GEngine->Client->Tick(DeltaTime);
#endif
		bIsFinished = MovieFinishEvent->Wait( 1 );
	}
}

/**
 * Checks to see if the movie has finished playing. Will return immediately
 *
 * @param MovieFilename MovieFilename to check against (should match the name passed to GameThreadPlayMovie). Empty string will match any movie
 * 
 * @return TRUE if the named movie has finished playing
 */
UBOOL FFullScreenMovieBink::GameThreadIsMovieFinished(const TCHAR* MovieFilename)
{
	// be default, the movie is not done
	UBOOL bIsMovieFinished = FALSE;

	// if either movie name is empty, then its a match
	if ((appStricmp(MovieFilename, TEXT("")) == 0) || 
		GameThreadMovieName == TEXT("") ||
		GameThreadMovieName == FFilename(MovieFilename).GetBaseFilename())
	{
		// check the status of the event, but don't wait for it
		bIsMovieFinished = MovieFinishEvent->Wait(0);
	}

	return bIsMovieFinished;
}

/**
 * Checks to see if the movie is playing. Will return immediately
 *
 * @param MovieFilename MovieFilename to check against (should match the name passed to GameThreadPlayMovie). Empty string will match any movie
 * 
 * @return TRUE if the named movie is playing
 */
UBOOL FFullScreenMovieBink::GameThreadIsMoviePlaying(const TCHAR* MovieFilename)
{
	// be default, the movie is not done
	UBOOL bIsMovieFinished = FALSE;

	// if either movie name is empty, then its a match
	if ((appStricmp(MovieFilename, TEXT("")) == 0) || 
		GameThreadMovieName == TEXT("") ||
		GameThreadMovieName == FFilename(MovieFilename).GetBaseFilename())
	{
		// check the status of the event, but don't wait for it
		bIsMovieFinished = !MovieFinishEvent->Wait(0);
	}

	return bIsMovieFinished;
}

/**
 * Get the name of the most recent movie played
 *
 * @return Name of the movie that was most recently played, or empty string if a movie hadn't been played
 */
FString FFullScreenMovieBink::GameThreadGetLastMovieName()
{
	return GameThreadMovieName;
}

/**
 * Kicks off a thread to control the startup movie sequence
 */
void FFullScreenMovieBink::GameThreadInitiateStartupSequence()
{
	if ( !ParseParam(appCmdLine(), TEXT("nostartupmovies")) )
	{
		if( StartupMovies.Num() > 0 )
		{
			// start playing the first available startup movie
			const FStartupMovie& StartupMovie = StartupMovies(0);
			GameThreadPlayMovie( MM_PlayOnceFromMemory, *StartupMovie.MovieName, 0  );
		}
	}
}

/**
 * Returns the current frame number of the movie (not thred synchronized in anyway, but it's okay 
 * if it's a little off
 */
INT FFullScreenMovieBink::GameThreadGetCurrentFrame()
{
	return CurrentFrame;
}

/**
 * Controls whether the movie player processes input.
 *
 * @param	bShouldMovieProcessInput	whether the movie should process input.
 */
void FFullScreenMovieBink::GameThreadToggleInputProcessing( UBOOL bShouldMovieProcessInput )
{
	debugf(NAME_DevMovie,TEXT("%s movie thread input processing"), bShouldMovieProcessInput ? TEXT("Disabling") : TEXT("Enabling"));

	if ( IsInGameThread() )
	{
		ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
			ToggleMovieInput,
			FFullScreenMovieBink*, MoviePlayer, this,
			UBOOL,bProcessInput,bShouldMovieProcessInput,
		{
			MoviePlayer->bEnableInputProcessing = bProcessInput;
		});
	}
	else
	{
		bEnableInputProcessing = bShouldMovieProcessInput;
	}
}


/**
 * Controls whether the movie  is hidden and if input will forcibly stop the movie from playing when hidden/
 *
 * @param	bHidden	whether the movie should be hidden
 */
void FFullScreenMovieBink::GameThreadSetMovieHidden( UBOOL bInHidden )
{
	ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
		ToggleMovieInput,
		FFullScreenMovieBink*, MoviePlayer, this,
		UBOOL,bHidden,bInHidden,
	{
		MoviePlayer->bHideRenderingOfMovie = bHidden;
	});

	FViewport::SetGameRenderingEnabled( bInHidden );	
}


/**
 * Tells the movie player to allow game rendering for a frame while still keeping the movie
 * playing. This will cover delays from render caching the first frame
 */
void FFullScreenMovieBink::GameThreadRequestDelayedStopMovie()
{
	// let game render again, but wait 2 frames before game presents
	FViewport::SetGameRenderingEnabled(TRUE, 2);
}


/**
 * If necessary endian swaps the memory representing the Bink movie.
 * This is only needed for in-memory movies as Bink automatically 
 * handles byte swapping for streaming directly from disk.
 *
 * @param Buffer Memory that contains the Bink movie
 * @param BufferSize Size of the memory
 *
 */
void FFullScreenMovieBink::EnsureMovieEndianess(void* Buffer, DWORD BufferSize)
{
#if !__INTEL_BYTE_ORDER__
	// need at least enough room for the signature
	if (BufferSize < 4)
	{
		return;
	}

	// Check to see if it is already endian swapped - this is required as this function gets called repeated on the same data
	// The unswapped signature is 'BIK' for Bink1 
	BYTE* Signature = static_cast<BYTE*>(Buffer);
	UBOOL bSwapped = FALSE;

	if ( Signature[0] != 'K' && Signature[1] != 'I' && Signature[2] != 'B' )
	{
		bSwapped = TRUE;
	}

	// The unswapped signature is 'KB2' for Bink1 
	if ( Signature[0] != '2' && Signature[1] != 'B' && Signature[2] != 'K' )
	{
		bSwapped = TRUE;
	}

	if( !bSwapped )
	{
		// endian swap the data
		DWORD* Data = static_cast<DWORD*>(Buffer);
		UINT   DataSize = BufferSize / 4;

		for (UINT DataIndex = 0; DataIndex < DataSize; ++DataIndex)
		{
			DWORD SourceData = Data[DataIndex];

			Data[DataIndex] = ((SourceData & 0x000000FF) << 24) |
				((SourceData & 0x0000FF00) <<  8) |
				((SourceData & 0x00FF0000) >>  8) |
				((SourceData & 0xFF000000) >> 24) ;
		}
	}
#endif
}

/**
 * Opens a Bink movie with streaming from disk
 *
 * @param MoviePath Path to the movie file that will be played
 *
 * @return TRUE if the movie was initialized properly, FALSE otherwise
 */
UBOOL FFullScreenMovieBink::OpenStreamedMovie(const TCHAR* MoviePath)
{
	FFilename Filename(MoviePath);
#if XBOX || PS3
	// some platform specific fun
	Filename = *GFileManager->GetPlatformFilepath(MoviePath);
#endif

	DWORD BinkOpenFlags = BINKSNDTRACK | BINKNOFRAMEBUFFERS;
#if DWTRIOVIZSDK
	BinkOpenFlags |=  BINKALPHA;
#endif

	// If zero, use Bink's default value for this movie
	if (BinkIOStreamReadBufferSize > 0)
	{
		BinkSetIOSize(BinkIOStreamReadBufferSize);
		BinkOpenFlags |= BINKIOSIZE;
	}

	if (GSuspendGameIODuringStreaming)
	{
		SuspendGameIO();
	}

	Bink = BinkOpen(TCHAR_TO_ANSI(*Filename), BinkOpenFlags);
 	if( !Bink )
 	{
 		// TTP 39919: If we encounter a read error during opening of a streamed movie we need to display a dirty-disk
 		//            error because we do not handle the case of a movie finishing before the streaming level.
 		appHandleIOFailure( *Filename );
 	}

	return (Bink != NULL);
}
/**
 * Loads the entire Bink movie into RAM and opens it for playback
 *
 * @param MovieFilename		Path to the movie file that will be played
 * @param Buffer			If NULL, opens movie from path, otherwise uses passed in memory
 *
 * @return TRUE if the movie was initialized properly, FALSE otherwise
 */
UBOOL FFullScreenMovieBink::OpenPreloadedMovie(const TCHAR* MoviePath,void* Buffer)
{
	UBOOL Result = TRUE;

	// If passed in buffer is NULL, fill it with data loaded below.
	if( Buffer == NULL )
	{
		FFilename Filename(MoviePath);
		FString LangExt(appGetLanguageExt());
		if( LangExt != TEXT("INT") )
		{
			FString LocFilename = Filename.GetLocalizedFilename(*LangExt);		
			if( GFileManager->FileSize(*LocFilename) != -1 )
			{
				Filename = LocFilename;
			}
		}

		if( GFileManager->FileSize(*Filename) != -1	
		&&	appLoadFileToArray(MemoryMovieBytes, *Filename) )
		{
			// Byte swap when loading from file. Note that streamed movies are handled automatically by Bink
			EnsureMovieEndianess(MemoryMovieBytes.GetData(), MemoryMovieBytes.Num());
			Buffer = MemoryMovieBytes.GetData();
		}
	}

	if( Buffer )
	{
		DWORD BinkOpenFlags = BINKSNDTRACK | BINKNOFRAMEBUFFERS | BINKFROMMEMORY;
#if DWTRIOVIZSDK
		BinkOpenFlags |=  BINKALPHA;
#endif
		// open the movie now in Buffer.
		Bink = BinkOpen( (char*) Buffer, BinkOpenFlags );
		if( !Bink )
		{
			MemoryMovieBytes.Empty();
			Result = FALSE;
		}
	}
	else
	{
		Result = FALSE;
	}

	return Result;
}

/**
 * Opens a Bink movie from the supplied memory.
 *
 * @param Buffer Memory that contains the Bink movie
 * @param BufferSize Size of the memory
 *
 * @return TRUE if the movie was initialized properly, FALSE otherwise
 */
UBOOL FFullScreenMovieBink::OpenSuppliedMovie(void* Buffer, DWORD BufferSize)
{
	EnsureMovieEndianess(Buffer, BufferSize);

	DWORD BinkOpenFlags = BINKSNDTRACK | BINKNOFRAMEBUFFERS | BINKFROMMEMORY;
#if DWTRIOVIZSDK
	BinkOpenFlags |=  BINKALPHA;
#endif

	Bink = BinkOpen(static_cast<const char*>(Buffer), BinkOpenFlags);
	return (Bink != NULL);
}

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
UBOOL FFullScreenMovieBink::PlayMovie(EMovieMode InMovieMode, const TCHAR* MoviePath, const TCHAR* MovieFilename, void* Buffer, DWORD BufferSize, INT StartFrame, INT bIsSkippable, INT InStartOfRenderingMovieFrame, INT InEndOfRenderingMovieFrame)
{
	check(!Bink);
	check(BinkRender);

	debugf(NAME_DevMovie, TEXT("Playing movie [%s]"), MovieFilename ? MovieFilename : TEXT("none"));

	if( BinkAudio )
	{
		// tell bink which audio tracks it should load 
		BinkAudio->SetSoundTracks( MovieFilename );	
	}

	ControlBackgroundCaching( FALSE );

	// strip path and add the movies directory to the base dir for the full path
	FString BaseMovieName = GetFilenameWithoutBinkExtension(MovieFilename, true);	

	// check to see if we're playing a startup movie
	INT StartupMovieIdx = INDEX_NONE;
	FStartupMovie* StartupMoviePtr = NULL;
	for( INT Idx=0; Idx < StartupMovies.Num(); Idx++ )
	{
		const FStartupMovie& StartupMovie = StartupMovies(Idx);
		if( StartupMovie.MovieName == BaseMovieName )
		{	
			// keep track of startup movie index
			StartupMovieIdx = Idx;
			StartupMoviePtr = &StartupMovies(Idx);
			// check to see if it has already been loaded into memory
			// we use the startup movie buffers instead of the supplied buffers when opening
			if( StartupMovie.Buffer != NULL )
			{
				// override the playback mode so that it reads from supplied memory				
				InMovieMode = (InMovieMode & MF_LoopPlayback) ? MM_LoopFromSuppliedMemory : MM_PlayOnceFromSuppliedMemory;
			}
			break;
		}
	}

	// load subtitle text file
	FString SubtitlePath = FString( MoviePath ) + TEXT("Movies\\") + BaseMovieName + TEXT(".txt");
	SubtitleStorage->Load(SubtitlePath);

	// use the region name from the startup movies if available
	FString FullPath = FString( MoviePath ) + TEXT("Movies\\") + BaseMovieName + TEXT(".bik");

	// open the movie based on playback type
	DWORD PlaybackType = InMovieMode & MF_TypeMask;
	switch(PlaybackType)
	{
	case MF_TypeStreamed:
		{
			OpenStreamedMovie(*FullPath);
		}
		break;
	case MF_TypePreloaded:
		{
			OpenPreloadedMovie(*FullPath, StartupMoviePtr ? StartupMoviePtr->Buffer : NULL);
		}
		break;
	case MF_TypeSupplied:
		{
			if( StartupMoviePtr &&
				StartupMoviePtr->Buffer != NULL )
			{
				// load from startup movie buffer
				OpenSuppliedMovie(StartupMoviePtr->Buffer, StartupMoviePtr->BufferSize);
				FIOSystem* IO = GIOManager->GetIOSystem(IOSYSTEM_GenericAsync); 
				IO->HintDoneWithFile(*FullPath);
			}
			else
			{
				// load from supplied buffer
				OpenSuppliedMovie(Buffer, BufferSize);				
			}
		}
		break;
	}
	// mark movie as loaded
	MemoryReadEvent->Trigger();

	// start preloading movies if we've just triggered the first startup movie
	if( StartupMovieIdx == 0 )
	{
		// init the startup sequence idx
		StartupSequenceStep = 0;
		// get async IO loader
		FIOSystem* IO = GIOManager->GetIOSystem(IOSYSTEM_GenericAsync); 
		// preload the other startup movies
		for( INT Idx = 1; Idx < StartupMovies.Num(); Idx++ )
		{
			FStartupMovie& StartupMovie = StartupMovies(Idx);
			// setup our info for this movie
			FString Pathname = appGameDir() + TEXT("Movies\\") + StartupMovie.MovieName + TEXT(".bik");
			INT FileSize = GFileManager->FileSize(*Pathname);
			// make sure it exists
			if( FileSize != -1 )
			{
				// allocate space for buffer
				StartupMovie.BufferSize = FileSize;
				StartupMovie.Buffer = appMalloc(StartupMovie.BufferSize);
				// increment load counter. this gets decremented when asynch load completes
				StartupMovie.LoadCounter.Increment();
				// load in background, which will happen before the game loads anything, so we don't need to block gamethread or anything
				IO->LoadData(Pathname, 0, StartupMovie.BufferSize, StartupMovie.Buffer, &StartupMovie.LoadCounter, AIOP_Normal);
			}
			else
			{
				// no movie file exists so no buffers allocated
				StartupMovie.Buffer = NULL;
				StartupMovie.BufferSize = 0;
			}
		}
	}

	// check for failure
	if( !Bink )
	{
		debugf(NAME_DevMovie, TEXT("WARNING: Failed to open/prepare movie '%s' for playback!"), MovieFilename);
		// reset any settings that we may have set
		StopCurrentAndPlayNext();		
		return FALSE;
	}

	
#if PS3
	// modify the rendering thread priority as appropriate for Bink playback
	extern void appPS3BinkSetRenderingThreadPriority(UBOOL bIsStartingMovie);
	appPS3BinkSetRenderingThreadPriority(TRUE);
#endif

	// get the length of the movie
	NumFrames = Bink->Frames;

	// go to the specified start frame
	check(StartFrame >= 0 && StartFrame < NumFrames);
	if (StartFrame > 0)
	{
		StartFrame = BinkGetKeyFrame(Bink, StartFrame, BINKGETKEYNEXT);
		BinkGoto(Bink, StartFrame, BINKGOTOQUICK);
	}

	// get the framerate of the movie
	BINKREALTIME RealTimeInfo;
	BinkGetRealtime(Bink, &RealTimeInfo, 0);
	FrameRate = static_cast<DOUBLE>(RealTimeInfo.FrameRateDiv) / static_cast<DOUBLE>(RealTimeInfo.FrameRate);

	// get ready for the movie to be decoded and rendered
	BinkRender->MovieInitRendering(Bink);
	
	if( BinkAudio )
	{
		// route the opened audio tracks to the appropriate audio channels
		BinkAudio->SetAudioChannels(Bink);
		// quiet the rest of the game
		BinkAudio->PushSilence();
	}
 
 	// remember the requested movie playback type
 	MovieMode = InMovieMode;
 
 	// remember the filename of the movie
 	MovieName = BaseMovieName;

	// mark movie as skippable
	bIsMovieSkippable = bIsSkippable;

	// setup the subtitles for this movie
	SubtitleStorage->ActivateMovie(BaseMovieName);

	// set the start and end frames for showing rendering
	StartOfRenderingMovieFrame = InStartOfRenderingMovieFrame;
	EndOfRenderingMovieFrame = InEndOfRenderingMovieFrame;

	// so if we are "fading in" the video audio with no actual visuals
	if( ( Bink->FrameNum <= static_cast<UINT>(StartOfRenderingMovieFrame) ) && ( StartOfRenderingMovieFrame != -1 ) )
	{
		FViewport::SetGameRenderingEnabled( TRUE );	
		bHideRenderingOfMovie = TRUE;
	}


#if USE_ASYNC_BINK
	if (Bink)
	{
		// start an async frame decode to prime the pump
		BinkRender->BeginUpdateTextures(Bink);
		BinkDecodeFrame(Bink);
	}
#endif

	return TRUE;
}

/**
 * Stops the movie and uninitializes the system. 
 * Can't stop until startup movies have finished rendering
 *
 * @param bForce	Whether we should force the movie to stop, even if it's not the last one in the sequence
 * @return TRUE if successful
 */
UBOOL FFullScreenMovieBink::StopMovie( UBOOL bForce/*=FALSE*/ )
{
	UBOOL Result = TRUE;

	// Make sure that movie is no longer in a paused state, especially since we may
	// be waiting for the movie to finish playing before bailing out.
	bIsPausedByUser = FALSE;
	bUserWantsToTogglePause = FALSE;

	// only allow a stop if we aren't waiting for the end of the startup sequence
	// two stops will really stop
	if( !bForce &&
		!bIsWaitingForEndOfRequiredMovies &&
		StartupSequenceStep != -1 &&
		StartupSequenceStep < (StartupMovies.Num()-1) )
	{
		bIsWaitingForEndOfRequiredMovies = TRUE;
		Result = FALSE;
	}
	else
	{
		StopCurrentAndPlayNext( !bForce );
	}

	return Result;
}

/**
 * Stops the movie if it is skipable
 */
void FFullScreenMovieBink::SkipMovie()
{
	if( bIsMovieSkippable )
	{
		StopMovie();
	}
}

/**
 * Read next bink frame to texture and handle IO failure
 */
void FFullScreenMovieBink::BinkDecodeFrame( BINK* Bink )
{
#if USE_ASYNC_BINK

	// kick off an async decode
	if (!BinkDoFrameAsync(Bink, GAsyncBinkThread1, GAsyncBinkThread2))
	{
		debugf(NAME_DevMovie, TEXT("Error decompressing Async Bink frame: %s"), ANSI_TO_TCHAR(BinkGetError()));
	}
	
#else

	// kick off synchronous decode
	BinkDoFrame(Bink);

#endif

	if (Bink->ReadError)
	{
		appHandleIOFailure(*(appGameDir() + TEXT("Movies\\") + MovieName + TEXT(".bik")));
	}	
}

/**
 * Perform per-frame processing, including rendering to the screen
 *
 * @return TRUE if movie is still playing, FALSE if movie ended or an error stopped the movie
 */
UBOOL FFullScreenMovieBink::PumpMovie()
{
	GBinkFrameSkipCount = 0;

	UBOOL Result=TRUE;
	
	check(Bink);
	check(BinkRender);

	// Check to see if the user has requested that the movie be PAUSED or UNPAUSED
	if( bUserWantsToTogglePause )
	{
		// We only allow the movie to be paused if we were given the OK by the creator
		if( ( MovieMode & MF_AllowUserToPause ) && !( MovieMode & MF_LoopPlayback ) )
		{
			if( bIsPausedByUser )
			{
				// Resume!
				BinkPause( Bink, FALSE );
				bIsPausedByUser = FALSE;
			}
			else
			{
				// Pause!
				BinkPause( Bink, TRUE );
				bIsPausedByUser = TRUE;
			}
		}

		// Reset bit that tells us that the user wants to toggle pause
		bUserWantsToTogglePause = FALSE;
	}


 	if( ( bIsPausedByUser || !BinkWait(Bink) ) &&
 		BinkRender->MovieInitRendering(Bink) )
	{
		if ( bUpdateAudio && Bink && BinkAudio )
		{
			if ( bMute )
			{
				debugf(NAME_DevMovie, TEXT("Mute bink movie"));
				BinkAudio->PushSilence();
				BinkAudio->Mute();
				BinkAudio->SetAudioChannels(Bink);
			}
			else
			{
				debugf(NAME_DevMovie, TEXT("Unmute bink movie"));
				BinkAudio->PopSilence();
				BinkAudio->UpdateVolumeLevels();
				BinkAudio->SetAudioChannels(Bink);
			}
			bUpdateAudio = FALSE;
		}

		// synch and lock textures to ready them for decoding the movie frame
		BinkRender->BeginUpdateTextures(Bink);

#if USE_ASYNC_BINK
		// make sure previous frame is finished decoding
		BinkDoFrameAsyncWait(Bink, -1);
#else
		// decode the current movie frame to the buffer textures and swap to the next ones
		BinkDecodeFrame( Bink );
#endif

		// Only decode frames if we're not currently paused
		if( !bIsPausedByUser )
		{
			// skip frames if necessary to catch up to audio
			while( BinkShouldSkip( Bink ) )
			{
#if USE_ASYNC_BINK
				// block until its complete
				BinkDoFrameAsyncWait( Bink, -1 );
#endif
				BinkNextFrame( Bink );
				BinkDecodeFrame( Bink );

				GBinkFrameSkipCount++;
			}
		}

		SCOPED_DRAW_EVENT(Event)(DEC_SCENE_ITEMS,TEXT("PumpMovie"));


		// Only decode frames if we're not currently paused
		if( !bIsPausedByUser )
		{
			// update the current frame
			BINKREALTIME RealTimeInfo;
			BinkGetRealtime(Bink, &RealTimeInfo, 0);
			CurrentFrame = RealTimeInfo.FrameNum;
		}

		// synch and unlock textures after decoding the movie frame
		BinkRender->EndUpdateTextures(Bink);


		FString Subtitle;
		DOUBLE Scale = 1000.0 * FrameRate;
		UINT ElapsedTime = static_cast<UINT>(CurrentFrame * Scale + 0.5);
		FString SubtitleID = SubtitleStorage->LookupSubtitle(ElapsedTime);
		if( SubtitleID.Len() )
		{
			Subtitle = Localize(TEXT("Subtitles"), *SubtitleID, TEXT("Subtitles"), NULL, TRUE);
			// if it wasn't found, then just use the Key
			if( Subtitle.Len() == 0 )
			{
				Subtitle = SubtitleID;
			}
		}


		// Grab the localized 'Game is Paused' text from the loc file if we need to
		FString PauseText;
		if( bIsPausedByUser )
		{
			PauseText = Localize( TEXT( "Subtitles" ), TEXT( "MovieIsPaused" ), TEXT( "Subtitles" ), NULL, FALSE );
		}

		if( bHideRenderingOfMovie == FALSE )
		{
			// are we playing the loading movie (better than checking GameTHread string variable)
			const UBOOL bIsLoadingMovie = MovieMode == MM_LoopFromSuppliedMemory;
#if DWTRIOVIZSDK
		if (DwTriovizImpl_NeedsFinalize() || DwTriovizImpl_IsTriovizActive()
	#if DWTRIOVIZSDK_FRAMEPACKING
			|| g_DwUseFramePacking
	#endif
		)
		{
			#if XBOX
				GSceneRenderTargets.BeginRenderingSceneColorLDR(RTUsage_FullOverwrite);
			#else
				GSceneRenderTargets.BeginRenderingSceneColor(RTUsage_FullOverwrite);
			#endif
			BinkRender->RenderFrame(
				Bink,
				*Subtitle,
				bIsPausedByUser ? *PauseText : NULL,
				bIsLoadingMovie,
				TextOverlays, TRUE);
			#if XBOX
				GSceneRenderTargets.FinishRenderingSceneColorLDR();
			#else
				GSceneRenderTargets.FinishRenderingSceneColor();
			#endif
			BinkRender->BeginRenderFrame();
			DwTriovizImpl_BinkRender(BinkRender->DwTrioviz_HasAlpha(), GSceneRenderTargets.GetBackBuffer(), 0, 0, BinkRender->GetViewport()->GetSizeX(), BinkRender->GetViewport()->GetSizeY());
		}
		else
		{
			// render the current frame
			BinkRender->BeginRenderFrame();
			BinkRender->RenderFrame(
				Bink,
				*Subtitle,
				bIsPausedByUser ? *PauseText : NULL,
				bIsLoadingMovie,
				TextOverlays);
		}
#else
		BinkRender->BeginRenderFrame();
		// are we playing the loading movie (better than checking GameTHread string variable)
		BinkRender->RenderFrame(
			Bink,
			*Subtitle,
			bIsPausedByUser ? *PauseText : NULL,
			bIsLoadingMovie,
			TextOverlays);
		BinkRender->EndRenderFrame();
#endif//DWTRIOVIZSDK
		}


		// Only advance to the next frame if we're not paused
		if( !bIsPausedByUser )
		{
			if( MovieMode & MF_LoopPlayback ||
				Bink->FrameNum < Bink->Frames )
			{
				//warnf( TEXT ( "FrameData Hid: %d  FrameNum: %d Start: %d End: %d" ), bHideRenderingOfMovie, Bink->FrameNum, StartOfRenderingMovieFrame, EndOfRenderingMovieFrame  );

				// if I am hiding it
				// and I should not be hiding it
				if( ( bHideRenderingOfMovie == TRUE ) && ( Bink->FrameNum >= static_cast<UINT>(StartOfRenderingMovieFrame) ) && ( StartOfRenderingMovieFrame != -1 ) )
				{
					// go ahead and show it
					//warnf( TEXT ( "SHOWING!!!!" ) );
					FViewport::SetGameRenderingEnabled( FALSE );	
					bHideRenderingOfMovie = FALSE;
					StartOfRenderingMovieFrame = -1; // set to -1 to not show again
				}

				// if I am not hiding it
				// and I want to be hiding it
				if( ( bHideRenderingOfMovie == FALSE ) && ( Bink->FrameNum >= static_cast<UINT>(EndOfRenderingMovieFrame) ) && ( EndOfRenderingMovieFrame != -1 ) )
				{
					// go ahead and hide it
					//warnf( TEXT ( "HIDING!!!!" ) );
					FViewport::SetGameRenderingEnabled( TRUE );	
					bHideRenderingOfMovie = TRUE;
					EndOfRenderingMovieFrame = -1; // set to -1 to not hide again
				}


				// increment to the next frame in the movie stream
				BinkNextFrame( Bink );

#if USE_ASYNC_BINK
				// kick off the next async decode
				BinkDecodeFrame( Bink );
#endif
			}
			else 
			{
				// end of playback for non-looping movies
				StopCurrentAndPlayNext();
				Result = FALSE;
			}
		}
	}



	return Result;
}

/**
 * Free any pre-allocated memory for movies in the startup list
 */
void FFullScreenMovieBink::FreeStartupMovieMemory()
{
	for( INT Idx=0; Idx < StartupMovies.Num(); Idx++ )
	{
		FStartupMovie& StartupMovie = StartupMovies(Idx);
		if( !StartupMovie.bNeverFreeBuffer &&
			StartupMovie.Buffer != NULL )
		{
			appFree(StartupMovie.Buffer);
			StartupMovie.Buffer = NULL;
			StartupMovie.BufferSize = 0;
		}
	}
}

/**
 * Stop current movie and proceed to the next startup movie (cleaning up any memory it can)
 *
 * @param bPlayNext	Whether we should proceed to the next startup movie
 */
void FFullScreenMovieBink::StopCurrentAndPlayNext( UBOOL bPlayNext/*=TRUE*/ )
{
	// Make sure that movie is no longer in a paused state.
	bIsPausedByUser = FALSE;
	bUserWantsToTogglePause = FALSE;

	// make sure the delay is no longer active
	MovieStopDelay = 0.0f;	

	// reset frame numbers
	CurrentFrame = NumFrames = -1;

	// don't want to do this multiple times
	if (Bink)
	{
		if( BinkRender )
		{
#if USE_ASYNC_BINK
			// make sure any async work is done before cleaning up
			BinkDoFrameAsyncWait(Bink, -1);
#endif

			// clean up rendering
			BinkRender->MovieCleanupRendering();

			// This will leave the device pointing to the engine FrontBuffer.
			// Otherwise, the memory that was used by the GMovieFrontBuffer
			// will continue to draw to the screen while it is being overwritten.
			BinkRender->BeginRenderFrame();
			BinkRender->RenderClear(FLinearColor::Black);
			BinkRender->EndRenderFrame();
		}

		// Uncomment this block to dump movie info from Bink.
		//BINKSUMMARY Summary;
		//BinkGetSummary(Bink, &Summary);

		//debugf(NAME_DevMovie, TEXT("Movie summary for %s:"), *MovieName);
		//debugf(NAME_DevMovie, TEXT("	Width of frames: %d."), Summary.Width);
		//debugf(NAME_DevMovie, TEXT("	Height of frames: %d."), Summary.Height);
		//debugf(NAME_DevMovie, TEXT("	total time (ms): %d."), Summary.TotalTime);
		//debugf(NAME_DevMovie, TEXT("	frame rate: %d."), Summary.FileFrameRate);
		//debugf(NAME_DevMovie, TEXT("	frame rate divisor: %d."), Summary.FileFrameRateDiv);
		//debugf(NAME_DevMovie, TEXT("	frame rate: %d."), Summary.FrameRate);
		//debugf(NAME_DevMovie, TEXT("	frame rate divisor: %d."), Summary.FrameRateDiv);
		//debugf(NAME_DevMovie, TEXT("	Time to open and prepare for decompression: %d."), Summary.TotalOpenTime);
		//debugf(NAME_DevMovie, TEXT("	Total Frames: %d."), Summary.TotalFrames);
		//debugf(NAME_DevMovie, TEXT("	Total Frames played: %d."), Summary.TotalPlayedFrames);
		//debugf(NAME_DevMovie, TEXT("	Total number of skipped frames: %d."), Summary.SkippedFrames);
		//debugf(NAME_DevMovie, TEXT("	Total number of skipped blits: %d."), Summary.SkippedBlits);
		//debugf(NAME_DevMovie, TEXT("	Total number of sound skips: %d."), Summary.SoundSkips);
		//debugf(NAME_DevMovie, TEXT("	Total time spent blitting: %d."), Summary.TotalBlitTime);
		//debugf(NAME_DevMovie, TEXT("	Total time spent reading: %d."), Summary.TotalReadTime);
		//debugf(NAME_DevMovie, TEXT("	Total time spent decompressing video: %d."), Summary.TotalVideoDecompTime);
		//debugf(NAME_DevMovie, TEXT("	Total time spent decompressing audio: %d."), Summary.TotalAudioDecompTime);
		//debugf(NAME_DevMovie, TEXT("	Total time spent reading while idle: %d."), Summary.TotalIdleReadTime);
		//debugf(NAME_DevMovie, TEXT("	Total time spent reading in background: %d."), Summary.TotalBackReadTime);
		//debugf(NAME_DevMovie, TEXT("	Total io speed (bytes/second): %d."), Summary.TotalReadSpeed);
		//debugf(NAME_DevMovie, TEXT("	Slowest single frame time (ms): %d."), Summary.SlowestFrameTime);
		//debugf(NAME_DevMovie, TEXT("	Second slowest single frame time (ms): %d."), Summary.Slowest2FrameTime);
		//debugf(NAME_DevMovie, TEXT("	Slowest single frame number: %d."), Summary.SlowestFrameNum);
		//debugf(NAME_DevMovie, TEXT("	Second slowest single frame number: %d."), Summary.Slowest2FrameNum);
		//debugf(NAME_DevMovie, TEXT("	Average data rate of the movie: %d."), Summary.AverageDataRate);
		//debugf(NAME_DevMovie, TEXT("	Average size of the frame: %d."), Summary.AverageFrameSize);
		//debugf(NAME_DevMovie, TEXT("	Highest amount of memory allocated: %d."), Summary.HighestMemAmount);
		//debugf(NAME_DevMovie, TEXT("	Total extra memory allocated: %d."), Summary.TotalIOMemory);
		//debugf(NAME_DevMovie, TEXT("	Highest extra memory actually used: %d."), Summary.HighestIOUsed);
		//debugf(NAME_DevMovie, TEXT("	Highest 1 second rate: %d."), Summary.Highest1SecRate);
		//debugf(NAME_DevMovie, TEXT("	Highest 1 second start frame: %d."), Summary.Highest1SecFrame);

		// clean up the bink player
		BinkClose(Bink);
		Bink = NULL;
	}

	// don't display overlays in next movie
	TextOverlays.Empty();

	// toss the in memory version of the movie
	MemoryMovieBytes.Empty();
	// allow game IO
	ResumeGameIO();

#if PS3
	// restore the render thread priority 
	extern void appPS3BinkSetRenderingThreadPriority(UBOOL bIsStartingMovie);
	appPS3BinkSetRenderingThreadPriority(FALSE);
#endif

	// see if there are more startup movies to process
	if( !bPlayNext || !ProcessNextStartupSequence() )
	{
		// done with startup movies if force stop
		StartupSequenceStep = -1;

		// toss any movie data that may be left for statup movies
		// freed if we force stop playback or if no more startup sequences to play
		FreeStartupMovieMemory();

		if( BinkAudio )
		{
			// restore engine's sound
			BinkAudio->PopSilence();
		}

		// let game render again
		FViewport::SetGameRenderingEnabled(TRUE);

		// mark that we are done playing the movie
		MovieFinishEvent->Trigger();
	}

	ControlBackgroundCaching( TRUE );
}

/**
 * Process the next step in the startup sequence.
 *
 * @return TRUE if a new movie was kicked off, FALSE otherwise
 */
UBOOL FFullScreenMovieBink::ProcessNextStartupSequence()
{
	UBOOL Result=TRUE;
	if( !StartupMovies.IsValidIndex(StartupSequenceStep) )
	{
		Result = FALSE;
	}
	else
	{
		// free memory for the now completed step
		FStartupMovie& OldStartupMovie = StartupMovies(StartupSequenceStep);	
		if( !OldStartupMovie.bNeverFreeBuffer &&
			OldStartupMovie.Buffer != NULL )
		{
			appFree(OldStartupMovie.Buffer);
			OldStartupMovie.Buffer = NULL;
			OldStartupMovie.BufferSize = 0;
		}
		// find another movie to play (first skip this one, then keep going until we have one		
		for( ++StartupSequenceStep; StartupSequenceStep < StartupMovies.Num(); ++StartupSequenceStep )
		{
			if( StartupMovies(StartupSequenceStep).Buffer != NULL )
			{
				// found valid movie index
				break;
			}
		}
		debugf(NAME_DevMovie, TEXT("Next movie? %d / %d"), StartupSequenceStep, StartupMovies.Num());
		if( !StartupMovies.IsValidIndex(StartupSequenceStep) )
		{
			StartupSequenceStep = -1;
			Result = FALSE;
		}
		else
		{ 
			UBOOL bLastMovie = StartupSequenceStep == (StartupMovies.Num()-1);

			// block for load to complete
			FStartupMovie& StartupMovie = StartupMovies(StartupSequenceStep);
			while( StartupMovie.LoadCounter.GetValue() != 0 )
			{
				appSleep(0);
			}

			// if waiting to play out the required movies, then skip over the last (looping until forever) movie
			if( bLastMovie && bIsWaitingForEndOfRequiredMovies ) 
			{
				// no longer waiting for startup movies to end
				bIsWaitingForEndOfRequiredMovies = FALSE;
				StartupSequenceStep = -1;
				return FALSE;
			}

			UBOOL bIsSkippable = SkippableMovieNames.FindItemIndex(StartupMovie.MovieName) != INDEX_NONE;
			// play next movie
			// if this is the last one, then kick off a looping playback, otherwise, single-shot
			PlayMovie(
				bLastMovie ? MM_LoopFromSuppliedMemory : MM_PlayOnceFromSuppliedMemory,
				*appGameDir(),
				*StartupMovie.MovieName,
				StartupMovie.Buffer,
				StartupMovie.BufferSize,
				0,
				bIsSkippable
				);
		}
	}
	return Result;
}

/**
 * Controls the background caching, keeping background caching paused while playing a Bink movie.
 * If the movie is streaming from the DVD, we don't want unnecessary seeks.
 * If the movie is playing from memory, we're normally loading something and still don't want unnecessary seeks.
 */
void FFullScreenMovieBink::ControlBackgroundCaching( UBOOL bEnable )
{
#if XBOX
	if ( !bEnable && !bHasPausedBackgroundCaching )
	{
		// Pause background caching
		XeControlHDDCaching( FALSE );
		bHasPausedBackgroundCaching = TRUE;
	}
	else if ( bEnable && bHasPausedBackgroundCaching )
	{
		// Resume background caching
		XeControlHDDCaching( TRUE );
		bHasPausedBackgroundCaching = FALSE;
	}
#endif
}

/**
 * Turns off game streaming and enabled movie streaming
 */
void FFullScreenMovieBink::SuspendGameIO()
{
//@todo sz - handle suspending io on other platforms
#if XBOX || PS3
	if( !GameIOSuspended )
	{
		FIOSystem* IO = GIOManager->GetIOSystem(IOSYSTEM_GenericAsync);
		IO->Suspend();

		if (Bink)
		{
			BinkControlBackgroundIO(Bink, BINKBGIORESUME);
		}

		GameIOSuspended = TRUE;

		GStreamingManager->DisableResourceStreaming();
	}
#endif
}

/**
 * Turns on game streaming and disables movie streaming
 */
void FFullScreenMovieBink::ResumeGameIO()
{
//@todo sz - handle suspending io on other platforms
#if XBOX || PS3
	if( GameIOSuspended )
	{
		if (Bink)
		{
			BinkControlBackgroundIO(Bink, BINKBGIOSUSPEND);
		}

		FIOSystem* IO = GIOManager->GetIOSystem(IOSYSTEM_GenericAsync);
		IO->Resume();

		GameIOSuspended = FALSE;

		GStreamingManager->EnableResourceStreaming();
	}
#endif
}

/** 
 * Constructor 
 */
FFullScreenMovieBink::FStartupMovie::FStartupMovie(const FString& InMovieName,UBOOL bInNeverFreeBuffer)
:	MovieName(InMovieName)
,	Buffer(NULL)
,	BufferSize(0)
,	bNeverFreeBuffer(bInNeverFreeBuffer)
{
}

/** 
 * Destructor
 */
FFullScreenMovieBink::FStartupMovie::~FStartupMovie()
{
	if( Buffer )
	{
		appFree(Buffer);
	}
}

void FFullScreenMovieBink::Mute(UBOOL InMute)
{
	bUpdateAudio = TRUE;
	bMute = InMute;
}


/**
 * Removes all overlays from displaying
 */
void FFullScreenMovieBink::GameThreadRemoveAllOverlays()
{
	ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
		RemoveAllOverlaysCommand,
		TArray<FFullScreenMovieOverlay>*, Overlays, &TextOverlays,
	{
		// clear out the overlays (maybe could call Reset
		Overlays->Empty();
	});
}

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
void FFullScreenMovieBink::GameThreadAddOverlay( UFont* Font, const FString& Text, FLOAT X, FLOAT Y, FLOAT ScaleX, FLOAT ScaleY, UBOOL bIsCentered, UBOOL bIsWrapped, FLOAT WrapWidth )
{
	if( Font != NULL )
	{
		check(Font->HasAllFlags(RF_RootSet));

		// make a new entry
		FFullScreenMovieOverlay Overlay;
		Overlay.Font = Font;
		Overlay.Text = Text;
		Overlay.X = X;
		Overlay.Y = Y;
		Overlay.ScaleX = ScaleX;
		Overlay.ScaleY = ScaleY;
		Overlay.bIsCentered = bIsCentered;
		Overlay.bIsWrapped = bIsWrapped;
		Overlay.WrapWidth = WrapWidth;

		ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
			RemoveAllOverlaysCommand,
			FFullScreenMovieOverlay, NewOverlay, Overlay,
			TArray<FFullScreenMovieOverlay>*, Overlays, &TextOverlays,
		{
			// add a new entry
			Overlays->AddItem(NewOverlay);
		});
	}
}


/*-----------------------------------------------------------------------------
	FBinkMovieAudio
-----------------------------------------------------------------------------*/

// constants
const INT BINK_MAX_VOLUME = 32768;
const FLOAT DEFAULT_VOLUME = 0.8f;

/**
 * Constructor
 */
FBinkMovieAudio::FBinkMovieAudio()
:	FoleyVolume(DEFAULT_VOLUME * BINK_MAX_VOLUME)
,	VoiceVolume(DEFAULT_VOLUME * BINK_MAX_VOLUME)
,	SavedMasterVolume(0.0f)

{
	// init bink audio
#if PS3
	UPS3AudioDevice::InitCellAudio();
	BinkSoundUseLibAudio(UPS3AudioDevice::NumSpeakers);
#elif XBOX
	BinkSoundUseXAudio2(UXAudio2Device::XAudio2);
#else
	UXAudio2Device::InitHardware();
	BinkSoundUseXAudio2(UXAudio2Device::XAudio2);
#endif
}

/**
 * Mute movie
 */
void FBinkMovieAudio::Mute()
{
	FoleyVolume = 0.0f;
	VoiceVolume = 0.0f;
}

/**
 * Updates volume levels for movie voice and effects tracks (called before playing movie)
 */
void FBinkMovieAudio::UpdateVolumeLevels()
{
	// Initialize so we use default volumes if profile doesn't exist
	FoleyVolume = DEFAULT_VOLUME;
	VoiceVolume = DEFAULT_VOLUME;

	if( GEngine )
	{
		UAudioDevice* AudioDevice = GEngine->Client ? GEngine->Client->GetAudioDevice() : NULL;
		if( AudioDevice )
		{
			FSoundClassProperties* MovieEffects = AudioDevice->GetCurrentSoundClass( TEXT( "MovieEffects" ) );
			if( MovieEffects )
			{
				FoleyVolume = MovieEffects->Volume * ( 1.0f / 0.8f );
			}
			FSoundClassProperties* MovieVoice = AudioDevice->GetCurrentSoundClass( TEXT( "MovieVoice" ) );
			if( MovieVoice )
			{
				VoiceVolume = MovieVoice->Volume * ( 1.0f / 0.8f );
			}
		}
	}

	FoleyVolume = Clamp<FLOAT>( FoleyVolume, 0.0f, 1.0f ) * BINK_MAX_VOLUME;
	VoiceVolume = Clamp<FLOAT>( VoiceVolume, 0.0f, 1.0f ) * BINK_MAX_VOLUME;
}

/**
 * Saves current volumes for all sound groups and sets volume to 0
 */
void FBinkMovieAudio::PushSilence()
{
#if XBOX
	if (SavedMasterVolume == -1.f)
	{
		FXMPHelper::GetXMPHelper()->MovieStarted();
	}
#endif
	extern FLOAT GGlobalAudioMultiplier;
	GGlobalAudioMultiplier = 0.f;
	SavedMasterVolume = 1.f;
}

/**
 * Restores previous volumes for all sound groups
 */
void FBinkMovieAudio::PopSilence()
{
	// if we saved the sound, restore it
	if (SavedMasterVolume != -1.0f)
	{
		extern FLOAT GGlobalAudioMultiplier;
		GGlobalAudioMultiplier = 1.f;
		SavedMasterVolume = -1.f;
#if XBOX
		FXMPHelper::GetXMPHelper()->MovieStopped();
#endif
	}
}

/**
 * Setup the bink audio tracks which should be loaded
 * Should be called before opening bink movie for playback
 */
void FBinkMovieAudio::SetSoundTracks( const TCHAR* MovieName )
{
	INT FrontLeft = SPEAKER_FrontLeft;
	INT FrontCenter = SPEAKER_RightSurround;
	FString LangExt = appGetLanguageExt();

	// If the movie name has _LOC in it, it has localised channels
	if( appStristr( MovieName, TEXT( "_LOC" ) ) != NULL )
	{
		if( LangExt == TEXT( "INT" ) && GEngine )
		{
#if 0
			// If we're running in censored mode, select the safe tracks
			if( !GEngine->bAllowMatureLanguage )
			{
				FrontLeft = 24;
				FrontCenter = 26;
			}
#endif
		}
		else if( LangExt == TEXT( "FRA" ) )
		{
			FrontLeft = 6;
			FrontCenter = 8;
		}
		else if( LangExt == TEXT( "ITA" ) )
		{
			FrontLeft = 9;
			FrontCenter = 11;
		}
		else if( LangExt == TEXT( "DEU" ) )
		{
			FrontLeft = 12;
			FrontCenter = 14;
		}
		else if( LangExt == TEXT( "ESN" ) )
		{
			FrontLeft = 15;
			FrontCenter = 17;
		}
		else if( LangExt == TEXT( "ESM" ) )
		{
			FrontLeft = 18;
			FrontCenter = 20;
		}
		else if( LangExt == TEXT( "JPN" ) )
		{
			FrontLeft = 21;
			FrontCenter = 23;
		}
		// Any other language just falls through
	}

	BinkSoundTracks[SPEAKER_FrontLeft] = FrontLeft;
	BinkSoundTracks[SPEAKER_FrontRight] = FrontLeft + 1;
	BinkSoundTracks[SPEAKER_FrontCenter] = FrontCenter;
	BinkSoundTracks[SPEAKER_LowFrequency] = SPEAKER_LeftSurround;
	BinkSoundTracks[SPEAKER_LeftSurround] = SPEAKER_FrontCenter;
	BinkSoundTracks[SPEAKER_RightSurround] = SPEAKER_LowFrequency;

    BinkSetSoundTrack( 6, BinkSoundTracks );

	debugf( NAME_DevMovie, TEXT( "Channel map: %d/%d/%d/%d/%d/%d" ), BinkSoundTracks[0], BinkSoundTracks[1], BinkSoundTracks[2], BinkSoundTracks[3], BinkSoundTracks[4], BinkSoundTracks[5] );
}

#if XBOX || _WINDOWS
/**
 * Handle setting of channel volumes for the XAudio2 system
 */
void FBinkMovieAudio::HandleXAudio2Volumes( BINK* Bink )
{
	const UINT NumBinkSoundTracks = ARRAY_COUNT( BinkSoundTracks );
	UINT SpeakerIndices[NumBinkSoundTracks] = { SPEAKER_FrontLeft, SPEAKER_FrontRight, SPEAKER_FrontCenter, SPEAKER_LowFrequency, SPEAKER_LeftSurround, SPEAKER_RightSurround, SPEAKER_LeftBack, SPEAKER_RightBack };

	INT BinkFoleyVolume = appTrunc( FoleyVolume );
	INT BinkVoiceVolume = appTrunc( VoiceVolume );

	if( Bink->NumTracks >= 6 )
	{
		// 6 mono channels making a 5.1 mix (from potentially more than 6 tracks for loc)
		for( UINT TrackIndex = 0; TrackIndex < SPEAKER_COUNT; TrackIndex++ )
		{
			S32 VolumeArray[NumBinkSoundTracks] = { 0 };

			for( INT SpeakerIndex = 0; SpeakerIndex < UXAudio2Device::NumSpeakers; SpeakerIndex++ )
			{
				FLOAT TrackVolume = ( TrackIndex == SPEAKER_FrontCenter ) ? BinkVoiceVolume : BinkFoleyVolume;
				VolumeArray[SpeakerIndex] = appTrunc( TrackVolume * UXAudio2Device::OutputMixMatrix[( SpeakerIndex * SPEAKER_COUNT ) + TrackIndex] );
			}

			BinkSetSpeakerVolumes( Bink, BinkSoundTracks[TrackIndex], SpeakerIndices, VolumeArray, UXAudio2Device::NumSpeakers );
		}
	}
	else if ( Bink->NumTracks == 1 )
	{
		// 1 stereo channels making a stereo mix
		S32 VolumeArray[NumBinkSoundTracks] = { 0 };

		VolumeArray[SPEAKER_FrontLeft] = BinkFoleyVolume;
		VolumeArray[SPEAKER_FrontRight] = BinkFoleyVolume;

		BinkSetSpeakerVolumes( Bink, 0, SpeakerIndices, VolumeArray, UXAudio2Device::NumSpeakers );
	}
}
#endif

#if PS3
/**
 * Handle setting of channel volumes for the MultiStream system
 */
void FBinkMovieAudio::HandleMultiStreamVolumes(BINK* Bink)
{
	const UINT NumBinkSoundTracks = ARRAY_COUNT(BinkSoundTracks);
	UINT SpeakerIndices[NumBinkSoundTracks] = { SPEAKER_FrontLeft, SPEAKER_FrontRight, SPEAKER_FrontCenter, SPEAKER_LowFrequency, SPEAKER_LeftSurround, SPEAKER_RightSurround };

	INT BinkFoleyVolume = appTrunc(FoleyVolume);
	INT BinkVoiceVolume = appTrunc(VoiceVolume);

	if ( Bink->NumTracks >= 6 && UPS3AudioDevice::NumSpeakers >= 6)
	{
		for( UINT TrackIndex = 0; TrackIndex < NumBinkSoundTracks; TrackIndex++ )
		{
			S32 VolumeArray[NumBinkSoundTracks] = { 0 };

			FLOAT TrackVolume = ( TrackIndex == SPEAKER_FrontCenter ) ? BinkVoiceVolume : BinkFoleyVolume;
			VolumeArray[TrackIndex] = appTrunc( TrackVolume );

			BinkSetSpeakerVolumes( Bink, BinkSoundTracks[TrackIndex], SpeakerIndices, VolumeArray, SPEAKER_COUNT );
		}
	}
	else
	{
		// for non-5.1, just set volume and center the pan for all tracks
		for (INT TrackIndex = 0; TrackIndex < Bink->NumTracks; TrackIndex++)
		{
			BinkSetVolume(Bink, TrackIndex, BinkFoleyVolume);
		}
	}
}
#endif

/**
 * Routes audio tracks to appropriate channels
 * Should be called after opening bink movie for playback
 *
 * @param Bink Handle to Bink interface
 */
void FBinkMovieAudio::SetAudioChannels(BINK* Bink)
{
	check(Bink);

#if PS3
	HandleMultiStreamVolumes( Bink );
#else
	HandleXAudio2Volumes( Bink );
#endif
}



/*-----------------------------------------------------------------------------
		FBinkMovieRenderClient
-----------------------------------------------------------------------------*/

// static global bink shaders 
FBinkMovieRenderClient::FInternalBinkShaders FBinkMovieRenderClient::BinkShaders;

/** 
 * Constructor
 */
FBinkMovieRenderClient::FBinkMovieRenderClient(FViewportClient* InViewportClient)
:	bInitializedMovieRendering(FALSE)
,	Viewport(NULL)
,	ViewportClient(InViewportClient)
{
	check(ViewportClient);

	// init viewport
	CreateViewport();
}

/** 
 * Destructor
 */
FBinkMovieRenderClient::~FBinkMovieRenderClient()
{
	// cleanup rendering resources
	MovieCleanupRendering();

	// cleanup viewports
	DestroyViewport();
}

/** 
 * Create the viewport for rendering the movies to
 */
void FBinkMovieRenderClient::CreateViewport()
{
	if( !Viewport )
	{
#if XBOX || PS3
		Viewport = GFullScreenMovieViewport;
		Viewport->SetViewportClient( ViewportClient );
#else
		check(GEngine && GEngine->Client && GEngine->GameViewport);
		Viewport = GEngine->GameViewport->Viewport;
#endif
		check(Viewport);
	}
}

/** 
 * Destroy the viewport that was created
 */
void FBinkMovieRenderClient::DestroyViewport()
{
#if XBOX || PS3
	if ( Viewport )
	{
		Viewport->SetViewportClient( NULL );
	}
#endif
	Viewport = NULL;
}

/**
 * Initialize internal rendering structures for a particular movie
 *
 * @param Bink Handle to Bink interface
 * @return TRUE if successful
 */
UBOOL FBinkMovieRenderClient::MovieInitRendering(BINK* Bink)
{
	check(Bink);

	if( !bInitializedMovieRendering )
	{
		// initialize internal bink shaders for rendering
		BinkShaders.Init();	
		// initialize internal bink textures
		BinkTextures.Init(Bink);
		// mark as initialized
		bInitializedMovieRendering = TRUE;
	}

	return TRUE;
}

/**
 * Teardown internal rendering structures for a particular movie
 */
void FBinkMovieRenderClient::MovieCleanupRendering()
{
	if( bInitializedMovieRendering )
	{
		BinkTextures.Clear();
		bInitializedMovieRendering = FALSE;
	}
}

/**
 * Start updating textures for a single movie frame
 *
 * @param Bink Handle to Bink interface
 */
void FBinkMovieRenderClient::BeginUpdateTextures(BINK* Bink)
{
	check( bInitializedMovieRendering );

#if BINK_USING_UNREAL_RHI
	// Handle restoring Bink movies if we lost the device during playback.
	if ( BinkTextures.TextureSet->bTriggerMovieRefresh )
	{
		if ( Bink->FrameNum > 1 )
		{
			// This will re-decompress the movie from the last keyframe up until the current frame.
 			BinkGoto(Bink, Bink->FrameNum-1, 0);
		}
		BinkTextures.TextureSet->bTriggerMovieRefresh = FALSE;
	}
#endif

	// lock bink buffer textures to decode into
	LockBinkTextures( BinkTextures.TextureSet );
	// register locked buffer texture ptrs 
	BinkRegisterFrameBuffers( Bink, &BinkTextures.TextureSet->bink_buffers );
	// wait for the GPU to finish with the previous frame textures
	WaitForBinkTextures( BinkTextures.TextureSet );
}

/**
 * End updating textures for a single movie frame
 *
 * @param Bink Handle to Bink interface
 */
void FBinkMovieRenderClient::EndUpdateTextures(BINK* Bink)
{
	check(bInitializedMovieRendering);
	// unlock bink buffer textures so that we can render with them
	UnlockBinkTextures( BinkTextures.TextureSet, Bink );
}

/**
 * Start rendering for a single movie frame
 */
void FBinkMovieRenderClient::BeginRenderFrame()
{
	SCOPED_DRAW_EVENT(Event)(DEC_SCENE_ITEMS,TEXT("BeginRenderFMV"));

	check(Viewport);
	Viewport->BeginRenderFrame();
}

/**
 * End rendering for a single movie frame
 */
void FBinkMovieRenderClient::EndRenderFrame()
{
	SCOPED_DRAW_EVENT(Event)(DEC_SCENE_ITEMS,TEXT("EndRenderFMV"));

	check(Viewport);
	Viewport->EndRenderFrame( TRUE, TRUE );
}



/**
 * Computes a vertical scale and offset value for image aspect compensation
 *
 * @param ImageWidth Width of image
 * @param ImageHeight Height of image
 * @param CanvasWidth Width of canvas
 * @param CanvasHeight Height of canvas
 * @param AspectOffset [Out] Pixels to offset to compensate for aspect
 * @param AspectScale [Out] Pixels to scale to compensate for aspect
 */
void FBinkMovieRenderClient::ComputeAspectOffsetAndScale( FLOAT ImageWidth, FLOAT ImageHeight, FLOAT CanvasWidth, FLOAT CanvasHeight, FVector2D& OutAspectOffset, FVector2D& OutAspectTextScale )
{
	const FLOAT ImageAspect = ImageWidth / ImageHeight;
	const FLOAT CanvasAspect = CanvasWidth / CanvasHeight;

	OutAspectOffset.X = 0.0f;
	OutAspectOffset.Y = 0.0f;
	OutAspectTextScale.X = 1.0f;
	OutAspectTextScale.Y = 1.0f;

	if( CanvasAspect < ImageAspect )
	{
		const FLOAT NewImageHeight = CanvasHeight / ImageAspect * CanvasAspect;

		OutAspectOffset.Y = ( CanvasHeight - NewImageHeight ) * 0.5;

// 		OutAspectTextScale.X = CanvasWidth / ImageWidth;
// 		OutAspectTextScale.Y = NewImageHeight / ImageHeight;

		// NOTE: Font size is automatically scaled for the canvas's WIDTH.  However, for tall aspect ratios where
		//   we're letterboxing a movie we want the text to be scaled appropriately for that
 		OutAspectTextScale.X = NewImageHeight / CanvasHeight;
 		OutAspectTextScale.Y = NewImageHeight / CanvasHeight;
	}
	else
	{
		const FLOAT NewImageWidth = CanvasWidth / CanvasAspect * ImageAspect;

		OutAspectOffset.X = ( CanvasWidth - NewImageWidth ) * 0.5;

// 		OutAspectTextScale.X = NewImageWidth / ImageWidth;
// 		OutAspectTextScale.Y = CanvasHeight / ImageHeight;
	}
}

/** 
 * Renders a word wrapped string to overlay the canvas
 */
/** The default offset of the outline box */
extern FIntRect UE3_DrawStringOutlineBoxOffset;

void FBinkMovieRenderClient::DisplayWrappedString( FCanvas* Canvas, const TCHAR* Text, UBOOL bCentered, UBOOL bTop, UFont* Font, FIntRect& Parms, const FLinearColor DrawColor )
{
	TArray<FWrappedStringElement> Lines;
	FTextSizingParameters RenderParms( 0.0f, 0.0f, Parms.Width(), 0.0f, Font, 0.0f );
	UCanvas::WrapString( RenderParms, 0, Text, Lines );

	if( Lines.Num() > 0 )
	{
		// Calculate the height of the text to be used for multiline spacing
		FLOAT FontHeight = Font->GetMaxCharHeight();
		FLOAT HeightTest = Canvas->GetRenderTarget()->GetSizeY();
		FontHeight *= Font->GetScalingFactor( HeightTest );

		if( !bTop )
		{
			Parms.Min.Y = Parms.Max.Y - ( Lines.Num() * ( INT )FontHeight );
		}

		FIntRect BackgroundBoxOffset = UE3_DrawStringOutlineBoxOffset;
		// Display subtitles from the bottom up
		for( INT Idx = 0; Idx < Lines.Num(); Idx++ )
		{
			const TCHAR* TextLine = *Lines( Idx ).Value;
			if (appStrlen(TextLine) > 0)
			{
				if( bCentered )
				{
					DrawStringOutlinedCenteredZ( Canvas, Parms.Min.X + ( Parms.Width() / 2 ), Parms.Min.Y, SUBTITLE_SCREEN_DEPTH_FOR_3D, 
						TextLine, Font, FLinearColor::White, GEngine->IsStereoscopic3D(), BackgroundBoxOffset);
				}
				else
				{
					DrawStringOutlinedZ( Canvas, Parms.Min.X, Parms.Min.Y, SUBTITLE_SCREEN_DEPTH_FOR_3D, TextLine, Font, DrawColor, 
						GEngine->IsStereoscopic3D(), BackgroundBoxOffset);
				}

				Parms.Min.Y += FontHeight;
				// Don't overlap subsequent boxes...
				BackgroundBoxOffset.Max.Y = BackgroundBoxOffset.Min.Y;
			}
		}
	}
}

/**
 * Render the latest decoded frame of the movie
 *
 * @param Bink Handle to Bink interface
 * @param Subtitle The subtitle string to render this frame (can be NULL or empty)
 * @param bLoadingMovie TRUE if we are rendering the loading movie
 * @param TextOverlays List of overlays to draw in a addition to any subtitles
 * @param bTriovizRenderToSceneColor TRUE if the movie should be rendered to a Trioviz scene color proxy
 */
void FBinkMovieRenderClient::RenderFrame(BINK* Bink, const TCHAR* Subtitle, const TCHAR* InPauseText, UBOOL bLoadingMovie, const TArray<FFullScreenMovieOverlay>& TextOverlays, UBOOL bTriovizRenderToSceneColor)
{
	SCOPED_DRAW_EVENT(Event)(DEC_SCENE_ITEMS,TEXT("RenderFMV"));

	check(bInitializedMovieRendering);
	check(Viewport);

	UINT FrameWidth, FrameHeight;
	UINT FrameOffsetX = 0;
	UINT FrameOffsetY = 0;

	// Scale FrameWidth or FrameHeight to preserve movie aspect ratio and calculate FrameOffset
	// to display frame at center of screen
	const FLOAT MovieAspectRatio = (FLOAT) Bink->Width / Bink->Height;
	const FLOAT ScreenAspectRatio = (FLOAT) Viewport->GetSizeX() / Viewport->GetSizeY();
	if ( ScreenAspectRatio < MovieAspectRatio )
	{
		FrameWidth = Viewport->GetSizeX();
		FrameHeight = appTrunc( Viewport->GetSizeX() / MovieAspectRatio );
		FrameOffsetY = (Viewport->GetSizeY() - FrameHeight) / 2;
	}
	else
	{
		FrameWidth = appTrunc( Viewport->GetSizeY() * MovieAspectRatio );
		FrameHeight = Viewport->GetSizeY();
		FrameOffsetX = (Viewport->GetSizeX() - FrameWidth) / 2;
	}

	// The loading screen image is usually 16:9, and will squash vertically (centered) to fit entirely within the
	// horizontal extents, so we need to account for this when positioning the overlay text.
	FVector2D AspectOffset, AspectTextScale;
	ComputeAspectOffsetAndScale( Bink->Width, Bink->Height, Viewport->GetSizeX(), Viewport->GetSizeY(), AspectOffset, AspectTextScale );

	// Clear front buffer, because we preserve movie aspect ratio during playback and we want to have black
	// stripes (at top and bottom of the screen). If we didn't do that we would have there last rendered scene
	RHIClear(TRUE,FLinearColor::Black,FALSE,0.0f,FALSE,0);	

	// draw quad to full viewport using cached bink textures
	DrawBinkTextures(
		BinkTextures.TextureSet,
		FrameWidth, 
		FrameHeight,
		+(FrameOffsetX * 2.0f / (FLOAT)Viewport->GetSizeX() - 1.0f),
		-(FrameOffsetY * 2.0f / (FLOAT)Viewport->GetSizeY() - 1.0f),
		+2.0f / (FLOAT)Viewport->GetSizeX(),
		-2.0f / (FLOAT)Viewport->GetSizeY(),
		1.0f,
		FALSE,
		TRUE
		);

	// only process subtitles if we have an engine
	if( GEngine && GEngine->Client )
	{
		UBOOL bNeedsNormalSubtitle = Subtitle && Subtitle[0] && GEngine->bSubtitlesEnabled;
		// render subtitle if we have one
		if( InPauseText != NULL || bLoadingMovie || bNeedsNormalSubtitle || TextOverlays.Num())
		{
			FRenderTarget* CanvasRenderTarget = Viewport;

#if DWTRIOVIZSDK
			static FDwSceneRenderTargetSceneColorProxy GDwSceneRenderTargetSceneColorProxy;			

			// do we want to render to scene color proxy instead of the viewport?
			if (bTriovizRenderToSceneColor)
			{
				CanvasRenderTarget = &GDwSceneRenderTargetSceneColorProxy;
			}
#endif

			// create a temporary FCanvas object with the viewport so it can get the screen size
			FCanvas Canvas(CanvasRenderTarget, NULL);

			INT Left = appTrunc( Viewport->GetSizeX() * 0.1f );
			INT Top = appTrunc( Viewport->GetSizeY() * 0.1f );
			INT Right = appTrunc( Viewport->GetSizeX() * 0.9f );
			INT Bottom = appTrunc( Viewport->GetSizeY() * 0.9f );

			if (bLoadingMovie || bNeedsNormalSubtitle)
			{
				FIntRect SafeZone( Left, Top, Right, Bottom );
				DisplayWrappedString( &Canvas, Subtitle, TRUE, FALSE, GEngine->SubtitleFont, SafeZone, FLinearColor::White );
			}

			// render overlays
			for (INT OverlayIndex = 0; OverlayIndex < TextOverlays.Num(); OverlayIndex++)
			{
				const FFullScreenMovieOverlay& Overlay = TextOverlays(OverlayIndex);

				// NOTE: We truncate to int here to reduce filtering problems when drawing the text (perfect texel -> pixel mapping)
				const INT PixelsX = appTrunc( AspectOffset.X + ( FLOAT )FrameWidth * Overlay.X );
				const INT PixelsY = appTrunc( AspectOffset.Y + ( FLOAT )FrameHeight * Overlay.Y );
				INT PixelsWrapWidth = appTrunc( ( FLOAT )Viewport->GetSizeX() * Overlay.WrapWidth );
				if( PixelsWrapWidth == 0 )
				{
					PixelsWrapWidth = Right - Left;
				}

				FLOAT CorrectedScaleX = AspectTextScale.X * Overlay.ScaleX;
				FLOAT CorrectedScaleY = AspectTextScale.Y * Overlay.ScaleY;
				UFont* Font = Overlay.Font;

				if (Overlay.bIsCentered)
				{
					FIntRect SafeZone( Left, Top, Right, Bottom );
					DisplayWrappedString( &Canvas, *Overlay.Text, TRUE, TRUE, Font, SafeZone, FLinearColor::White  );
				}
				else
				{
					FIntRect SafeZone( 
						Max<INT>( Left, PixelsX ), 
						Max<INT>( Top, PixelsY ), 
						Min<INT>( Left + PixelsWrapWidth, Right ),
						Bottom
						);

					DisplayWrappedString( &Canvas, *Overlay.Text, FALSE, TRUE, Font, SafeZone, FLinearColor::White );
				}
			}

			// Draw 'Game is Paused' text if we need to do that
			if( InPauseText != NULL )
			{
				FIntRect SafeZone( Left, Top, Right, Bottom );
				FIntRect BackgroundBoxOffset(2, 2, 4, 4);
				DrawStringOutlinedCenteredZ( &Canvas, SafeZone.Min.X + ( SafeZone.Width() / 2 ), SafeZone.Min.Y + ( SafeZone.Height() / 2 ) - 24, 
					SUBTITLE_SCREEN_DEPTH_FOR_3D, InPauseText, GEngine->SubtitleFont, FLinearColor::White, GEngine->IsStereoscopic3D(), BackgroundBoxOffset );
			}

			// A forced flush is now required
			Canvas.Flush();
		}
	}
}

/**
 * Render a black frame
 */
void FBinkMovieRenderClient::RenderClear(const FLinearColor& ClearColor)
{
	RHIClear(TRUE,ClearColor,FALSE,0.0f,FALSE,0);	
}

/**
 * Tick the viewport iot to handle input processing
 *
 * @param DeltaTime	Game time passed since the last call.
 */
void FBinkMovieRenderClient::Tick(FLOAT DeltaTime)
{
	if( Viewport )
	{
#if CONSOLE
		// The viewport will be created before the client on consoles, so process its input directly.
		// On consoles it's assumed that ProcessInput only access local variables or thread-safe system functions, and calls functions on
		// the Bink viewport client, so it's safe to call from here in the rendering thread.
		Viewport->ProcessInput(DeltaTime);
#endif
	}
}

/** 
 * Creates the internal bink frame buffer textures
 */
void FBinkMovieRenderClient::FInternalBinkTextures::Init(HBINK Bink)
{
	if( !bInitialized )
	{
		check(Bink);

		// allocate the texture set info and zero it
		TextureSet = new FBinkTextureSet;
		appMemzero(&TextureSet->bink_buffers,sizeof(TextureSet->bink_buffers));
		appMemzero(TextureSet->textures,sizeof(TextureSet->textures));
		// get information about the YUV frame buffers required by Bink
		BinkGetFrameBuffersInfo( Bink, &TextureSet->bink_buffers );
		// create the YUV frame buffers required for decoding by Bink
		CreateBinkTextures( TextureSet );
		bInitialized=TRUE;
	}

}

/**
 * Clears the internal bink frame buffer textures
 */
void FBinkMovieRenderClient::FInternalBinkTextures::Clear()
{
	if( bInitialized )
	{
		FreeBinkTextures( TextureSet );
		delete TextureSet;
		TextureSet = NULL;
		bInitialized=FALSE;
	}
}

/** 
 * Creates the internal bink shaders
 */
void FBinkMovieRenderClient::FInternalBinkShaders::Init()
{
	if( !bInitialized )
	{
		// shaders for converting from YUV(+Alpha) to RGB(+Alpha)
		CreateBinkShaders();
		bInitialized=TRUE;
	}
}

/**
 * Frees the internal bink shaders
 */
void FBinkMovieRenderClient::FInternalBinkShaders::Clear()
{
#if PS3
	if (GIsRequestingExit)
	{
		return;
	}
#endif
	if( bInitialized )
	{
		FreeBinkShaders();
		bInitialized=FALSE;
	}
}

#endif //USE_BINK_CODEC
