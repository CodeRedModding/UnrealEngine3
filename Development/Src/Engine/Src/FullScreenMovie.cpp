/*=============================================================================
	FullscreenMovie.cpp: Fullscreen movie playback implementation
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"

/*-----------------------------------------------------------------------------
FFullScreenMovieBink
-----------------------------------------------------------------------------*/

#if USE_BINK_CODEC
	#include "../Bink/Src/FullScreenMovieBink.inl"
#endif

/*-----------------------------------------------------------------------------
FFullScreenMovieFallback
-----------------------------------------------------------------------------*/

/** 
* Constructor
*/
FFullScreenMovieFallback::FFullScreenMovieFallback(UBOOL bUseSound)
{

}

/** 
* Perform one-time initialization and create instance
*
* @param bUseSound - TRUE if sound should be enabled for movie playback
* @return new instance if successful
*/
FFullScreenMovieSupport* FFullScreenMovieFallback::StaticInitialize(UBOOL bUseSound)
{
	static FFullScreenMovieFallback* StaticInstance = NULL;
	if( !StaticInstance )
	{
		StaticInstance = new FFullScreenMovieFallback(bUseSound);
	}
	return StaticInstance;
}

/**
* Pure virtual that must be overloaded by the inheriting class. It will
* be called from within UnLevTick.cpp after ticking all actors.
*
* @param DeltaTime	Game time passed since the last call.
*/
void FFullScreenMovieFallback::Tick(FLOAT DeltaTime)
{
	
}

/**
* Pure virtual that must be overloaded by the inheriting class. It is
* used to determine whether an object is ready to be ticked. This is 
* required for example for all UObject derived classes as they might be
* loaded async and therefore won't be ready immediately.
*
* @return	TRUE if class is ready to be ticked, FALSE otherwise.
*/
UBOOL FFullScreenMovieFallback::IsTickable() const
{
	// no need to tick the fallback
	return FALSE;
}

/**
* Kick off a movie play from the game thread
*
* @param InMovieMode How to play the movie (usually MM_PlayOnceFromStream or MM_LoopFromMemory).
* @param MovieFilename Path of the movie to play in its entirety
* @param StartFrame Optional frame number to start on
* @param InStartOfRenderingMovieFrame When the fading in from just audio to audio and video should occur
* @param InEndOfRenderingMovieFrame When the fading from audio and video to just audio should occur
*/
void FFullScreenMovieFallback::GameThreadPlayMovie(EMovieMode InMovieMode, const TCHAR* MovieFilename, INT StartFrame, INT InStartOfRenderingMovieFrame, INT InEndOfRenderingMovieFrame)
{

}

/**
* Stops the currently playing movie
*
* @param DelayInSeconds Will delay the stopping of the movie for this many seconds. If zero, this function will wait until the movie stops before returning.
* @param bWaitForMovie if TRUE then wait until the movie finish event triggers
* @param bForceStop if TRUE then non-skippable movies and startup movies are forced to stop
*/
void FFullScreenMovieFallback::GameThreadStopMovie(FLOAT DelayInSeconds,UBOOL bWaitForMovie,UBOOL bForceStop)
{

}

/**
* Block game thread until movie is complete (must have been started
* with GameThreadPlayMovie or it may never return)
*/
void FFullScreenMovieFallback::GameThreadWaitForMovie()
{

}

/**
* Checks to see if the movie has finished playing. Will return immediately
*
* @param MovieFilename MovieFilename to check against (should match the name passed to GameThreadPlayMovie). Empty string will match any movie
* 
* @return TRUE if the named movie has finished playing
*/
UBOOL FFullScreenMovieFallback::GameThreadIsMovieFinished(const TCHAR* MovieFilename)
{
	return TRUE;
}

/**
* Checks to see if the movie is playing. Will return immediately
*
* @param MovieFilename MovieFilename to check against (should match the name passed to GameThreadPlayMovie). Empty string will match any movie
* 
* @return TRUE if the named movie is playing
*/
UBOOL FFullScreenMovieFallback::GameThreadIsMoviePlaying(const TCHAR* MovieFilename)
{
	return FALSE;
}

/**
* Get the name of the most recent movie played
*
* @return Name of the movie that was most recently played, or empty string if a movie hadn't been played
*/
FString FFullScreenMovieFallback::GameThreadGetLastMovieName()
{
	return FString(TEXT(""));
}

/**
* Kicks off a thread to control the startup movie sequence
*/
void FFullScreenMovieFallback::GameThreadInitiateStartupSequence()
{

}

/**
* Returns the current frame number of the movie (not thred synchronized in anyway, but it's okay 
* if it's a little off
*/
INT FFullScreenMovieFallback::GameThreadGetCurrentFrame()
{
	return 0;
}

/**
* Releases any dynamic resources. This is needed for flushing resources during device reset on d3d.
*/
void FFullScreenMovieFallback::ReleaseDynamicResources()
{

}



