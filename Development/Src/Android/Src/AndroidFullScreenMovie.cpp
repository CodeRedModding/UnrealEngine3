/*=============================================================================
	AndroidFullScreenMovie.cpp
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/
#include <jni.h>
#include <pthread.h>
#include <semaphore.h>

#include "EnginePrivate.h"
#include "AndroidFullScreenMovie.h"
#include "AndroidJNI.h"

/*-----------------------------------------------------------------------------
	FAndroidFullScreenMovie
-----------------------------------------------------------------------------*/

FAndroidFullScreenMovie::FStartupMovie::FStartupMovie(const FString& InMovieName)
:	MovieName(InMovieName)
{
}

FAndroidFullScreenMovie::FAndroidFullScreenMovie()
{
	bDisabledGameViewport	= FALSE;
	bProcessSequenceDone	= FALSE;
	bIsMoviePlaying			= FALSE;
	LastMovieStartTime		= 0.0;

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
		new(StartupMovies) FStartupMovie(TheMovieName);		
	}
}



////////////////////////////////////
/// Function: 
///    FAndroidFullScreenMovie::StaticInitialize
///    
/// Specifiers: 
///    [FFullScreenMovieSupport*] - 
///    
/// Parameters: 
///    [void] - 
///    
/// Description: 
///    
///    
////////////////////////////////////
FFullScreenMovieSupport* FAndroidFullScreenMovie::StaticInitialize(UBOOL bUseSound)
{
	static FAndroidFullScreenMovie* StaticInstance = NULL;
	if( !StaticInstance )
	{
		//just always use sound
		StaticInstance = new FAndroidFullScreenMovie();
	}
	return StaticInstance;
}


////////////////////////////////////
/// Function: 
///    FAndroidFullScreenMovie::ProcessNextStartupSequence
///    
/// Specifiers: 
///    [UBOOL] - 
///    
/// Parameters: 
///    [void] - 
///    
/// Description: 
///    
///    
////////////////////////////////////
UBOOL FAndroidFullScreenMovie::ProcessNextStartupSequence()
{	
	if( !StartupMovies.IsValidIndex(StartupSequenceStep) )
	{
		bProcessSequenceDone = TRUE;
		return FALSE;
	}
	else
	{
		StartupSequenceStep++;
		debugf(NAME_DevMovie, TEXT("ProcessNextStartupSequence? %d / %d"), StartupSequenceStep, StartupMovies.Num());
		if( !StartupMovies.IsValidIndex(StartupSequenceStep) )
		{
			bProcessSequenceDone = TRUE;
			StartupSequenceStep = -1;
		}
		else
		{ 			
			PlayMovie( *StartupMovies(StartupSequenceStep).MovieName, TRUE );
			return TRUE;
		}
	}

	return FALSE;
}

////////////////////////////////////
/// Function: 
///    FAndroidFullScreenMovie::Tick
///    
/// Specifiers: 
///    [void] - 
///    
/// Parameters: 
///    [void] - 
///    
/// Description: 
///    
///    
////////////////////////////////////
void FAndroidFullScreenMovie::Tick(FLOAT DeltaTime)
{
	//verify whether we need to restart the game rendering
	if( bDisabledGameViewport && !GameThreadIsMoviePlaying(TEXT("")) && bProcessSequenceDone )
	{		
		debugf(NAME_DevMovie, TEXT("Renabling Game Viewport Rendering") );
		// make sure game isn't rendered while movie is playing
		FViewport::SetGameRenderingEnabled(TRUE);	
		// reflag it
		bDisabledGameViewport = FALSE;
	}
}

////////////////////////////////////
/// Function: 
///    FAndroidFullScreenMovie::IsTickable
///    
/// Specifiers: 
///    [UBOOL] - 
///    
/// Parameters: 
///    [void] - 
///    
/// Description: 
///    
///    
////////////////////////////////////
UBOOL FAndroidFullScreenMovie::IsTickable() const
{	
	return TRUE;
}

////////////////////////////////////
/// Function: 
///    FAndroidFullScreenMovie::PlayMovie
///    
/// Specifiers: 
///    [UBOOL] - 
///    
/// Parameters: 
///    [const TCHAR* MovieFilename] - 
///    [INT bIsSkippable] - 
///    
/// Description: 
///    
///    
////////////////////////////////////
UBOOL FAndroidFullScreenMovie::PlayMovie(const TCHAR* MovieFilename, INT bIsSkippable)
{	
	LastMovieStartTime = appSeconds();

	// reset it to playing in case
	bIsMoviePlaying = TRUE;

	CallJava_StartMovie( MovieFilename );

	return TRUE;
}

////////////////////////////////////
/// Function: 
///    FAndroidFullScreenMovie::GameThreadPlayMovie
///    
/// Specifiers: 
///    [void] - 
///    
/// Parameters: 
///    [EMovieMode InMovieMode] - 
///    [const TCHAR* MovieFilename] - 
///    [INT StartFrame] - 
///    [INT InStartOfRenderingMovieFrame] - 
///    [INT InEndOfRenderingMovieFrame] - 
///    [UBOOL bIsSkippable] - 
///    
/// Description: 
///    
///    
////////////////////////////////////
void FAndroidFullScreenMovie::GameThreadPlayMovie(EMovieMode InMovieMode, const TCHAR* MovieFilename, INT StartFrame, INT InStartOfRenderingMovieFrame, INT InEndOfRenderingMovieFrame)
{
	bIsMoviePlaying			= TRUE;
	bDisabledGameViewport	= TRUE;	

	// make sure rendering thread is done rendering
	FlushRenderingCommands();

	// make sure game isn't rendered while movie is playing
	FViewport::SetGameRenderingEnabled(FALSE);	

	debugf(NAME_DevMovie, TEXT("Playing movie [%s]"), MovieFilename ? MovieFilename : TEXT("none"));

	// strip path and add the movies directory to the base dir for the full path
	FString BaseMovieName = FFilename(MovieFilename).GetBaseFilename();	

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
		}
	}

	// start preloading movies if we've just triggered the first startup movie
	if( StartupMovieIdx == 0 )
	{
		// init the startup sequence idx
		StartupSequenceStep = 0;		
	}
  
 	// remember the filename of the movie
 	MovieName = BaseMovieName;

	// queue playback parameters to render thread
	struct FPlayMovieParams
	{
		FString MovieName;
		UBOOL bIsSkippable;
	};

	FPlayMovieParams PlayMovieParams = 
	{
		MovieName, 
		FALSE
	};
	ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
		PlayRenderThreadMovie,
		FPlayMovieParams, Params, PlayMovieParams,
		FAndroidFullScreenMovie*, MoviePlayer, this,
	{
		// start playing the movie
		MoviePlayer->PlayMovie(
			*Params.MovieName, 
			Params.bIsSkippable
			);
	});	
}

////////////////////////////////////
/// Function: 
///    FAndroidFullScreenMovie::GameThreadStopMovie
///    
/// Specifiers: 
///    [void] - 
///    
/// Parameters: 
///    [FLOAT DelayInSeconds] - 
///    [UBOOL bWaitForMovie] - 
///    [UBOOL bForceStop] - 
///    
/// Description: 
///    
///    
////////////////////////////////////
void FAndroidFullScreenMovie::GameThreadStopMovie(FLOAT DelayInSeconds,UBOOL bWaitForMovie,UBOOL bForceStop)
{
	debugf(NAME_DevMovie, TEXT("Game Thread Stopping movie %s"),*MovieName);

	if( appSeconds() - LastMovieStartTime < 1.0f )
	{
		return;
	}

	CallJava_StopMovie();
	bIsMoviePlaying = FALSE;
}

////////////////////////////////////
/// Function: 
///    FAndroidFullScreenMovie::GameThreadWaitForMovie
///    
/// Specifiers: 
///    [void] - 
///    
/// Parameters: 
///    [void] - 
///    
/// Description: 
///    
///    
////////////////////////////////////
void FAndroidFullScreenMovie::GameThreadWaitForMovie()
{	
	debugf(NAME_DevMovie, TEXT("====================="));
	debugf(NAME_DevMovie, TEXT("Waiting for movie"));

	// wait for it to complete
	while ( (bIsMoviePlaying || !bProcessSequenceDone) && !GIsRequestingExit )
	{
		// Compute the time since the last tick.
		static DOUBLE LastTickTime	= appSeconds();
		const DOUBLE CurrentTime	= appSeconds();
		const FLOAT DeltaTime		= CurrentTime - LastTickTime;
		LastTickTime				= CurrentTime;

		// some basic input processing
		check(GEngine->Client);
		GEngine->Client->Tick(DeltaTime);

		appSleep(0.1f);

		// iphone isn't threaded
		if( !bIsMoviePlaying )
		{
			ProcessNextStartupSequence();
		}
	}

	debugf(NAME_DevMovie, TEXT("COMPLETED: Waiting for movie"));
	debugf(NAME_DevMovie, TEXT("====================="));
}

////////////////////////////////////
/// Function: 
///    FAndroidFullScreenMovie::GameThreadIsMovieFinished
///    
/// Specifiers: 
///    [UBOOL] - 
///    
/// Parameters: 
///    [void] - 
///    
/// Description: 
///    
///    
////////////////////////////////////
UBOOL FAndroidFullScreenMovie::GameThreadIsMovieFinished(const TCHAR* MovieFilename)
{
	return TRUE;
}

////////////////////////////////////
/// Function: 
///    FAndroidFullScreenMovie::GameThreadIsMoviePlaying
///    
/// Specifiers: 
///    [UBOOL] - 
///    
/// Parameters: 
///    [void] - 
///    
/// Description: 
///    
///    
////////////////////////////////////
UBOOL FAndroidFullScreenMovie::GameThreadIsMoviePlaying(const TCHAR* MovieFilename)
{		
	return bIsMoviePlaying;
}

////////////////////////////////////
/// Function: 
///    FAndroidFullScreenMovie::GameThreadGetLastMovieName
///    
/// Specifiers: 
///    [FString] - 
///    
/// Parameters: 
///    [void] - 
///    
/// Description: 
///    
///    
////////////////////////////////////
FString FAndroidFullScreenMovie::GameThreadGetLastMovieName()
{
	return MovieName;
}

////////////////////////////////////
/// Function: 
///    FAndroidFullScreenMovie::GameThreadInitiateStartupSequence
///    
/// Specifiers: 
///    [void] - 
///    
/// Parameters: 
///    [void] - 
///    
/// Description: 
///    
///    
////////////////////////////////////
UBOOL GSkipStartupMovies = FALSE;
void FAndroidFullScreenMovie::GameThreadInitiateStartupSequence()
{
	if ( !GSkipStartupMovies && !ParseParam(appCmdLine(), TEXT("nostartupmovies")) )
	{
		if( StartupMovies.Num() > 0 )
		{
			// start playing the first available startup movie
			const FStartupMovie& StartupMovie = StartupMovies(0);
			GameThreadPlayMovie( MM_PlayOnceFromMemory, *StartupMovie.MovieName, 0  );
		}
	}
}

////////////////////////////////////
/// Function: 
///    FAndroidFullScreenMovie::GameThreadGetCurrentFrame
///    
/// Specifiers: 
///    [INT] - 
///    
/// Parameters: 
///    [void] - 
///    
/// Description: 
///    
///    
////////////////////////////////////
INT FAndroidFullScreenMovie::GameThreadGetCurrentFrame()
{
	return 0;
}

////////////////////////////////////
/// Function: 
///    FAndroidFullScreenMovie::ReleaseDynamicResources
///    
/// Specifiers: 
///    [void] - 
///    
/// Parameters: 
///    [void] - 
///    
/// Description: 
///    
///    
////////////////////////////////////
void FAndroidFullScreenMovie::ReleaseDynamicResources()
{
	// nothing to release
}

////////////////////////////////////
/// Function: 
///    FAndroidFullScreenMovie::InputKey
///    
/// Specifiers: 
///    [UBOOL] - 
///    
/// Parameters: 
///    [FViewport* Viewport] - 
///    [INT ControllerId] - 
///    [FName Key] - 
///    [EInputEvent Event] - 
///    [FLOAT AmountDepressed] - 
///    [UBOOL bGamepad] - 
///    
/// Description: 
///    
///    
////////////////////////////////////
UBOOL FAndroidFullScreenMovie::InputKey(FViewport* Viewport,INT ControllerId,FName Key,EInputEvent Event,FLOAT AmountDepressed,UBOOL bGamepad)
{
	UBOOL bIsMoviePlay = GameThreadIsMoviePlaying(TEXT(""));
	if( bIsMoviePlay && Event == IE_Released )
	{
		debugf(TEXT("Attempting to skip movie"));
		GameThreadStopMovie();
		return TRUE;
	}

	return FALSE;
}

UBOOL FAndroidFullScreenMovie::InputTouch(FViewport* Viewport, INT ControllerId, UINT Handle, BYTE Type, FVector2D TouchLocation, DOUBLE DeviceTimestamp, UINT TouchpadIndex)
{
	UBOOL bIsMoviePlay = GameThreadIsMoviePlaying(TEXT(""));
	if( bEnableInputProcessing && bIsMoviePlay )
	{
		SkipMovie();
		return TRUE;
	}

	return FALSE;
}

/**
 * Controls whether the movie player processes input.
 *
 * @param	bShouldMovieProcessInput	whether the movie should process input.
 */
void FAndroidFullScreenMovie::GameThreadToggleInputProcessing(UBOOL bShouldMovieProcessInput)
{
	bEnableInputProcessing = bShouldMovieProcessInput;
}

// Skips movie if in skippable list
void FAndroidFullScreenMovie::SkipMovie()
{
	debugf(TEXT("Attempting to skip movie"));
	for (int i = 0; i < SkippableMovieNames.Num(); ++i)
	{
		if (MovieName == SkippableMovieNames(i))
		{
			GameThreadStopMovie();
			break;
		}
	}
}