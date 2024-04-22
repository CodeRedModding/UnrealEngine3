/*=============================================================================
	AndroidFullScreenMovie.h
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef _MOBILEFULLSCREENMOVIE_H_
#define _MOBILEFULLSCREENMOVIE_H_

////////////////////////////////////
/// Class: 
///    class FAndroidFullScreenMovie : public FFullScreenMovieSupport
///    
/// Description: 
///    
///    
////////////////////////////////////
class FAndroidFullScreenMovie : public FFullScreenMovieSupport
{
public:

	static FFullScreenMovieSupport* StaticInitialize(UBOOL bUseSound);
	
	//Tickables
	virtual void Tick(FLOAT DeltaTime);
	virtual UBOOL IsTickable() const;

	// FFullScreenMovieSupport functions
	virtual void GameThreadPlayMovie(EMovieMode InMovieMode, const TCHAR* MovieFilename, INT StartFrame=0, INT InStartOfRenderingMovieFrame=-1, INT InEndOfRenderingMovieFrame=-1);
	virtual void GameThreadStopMovie(FLOAT DelayInSeconds=0.0f,UBOOL bWaitForMovie=TRUE,UBOOL bForceStop=FALSE);
	virtual void GameThreadWaitForMovie();
	virtual UBOOL GameThreadIsMovieFinished(const TCHAR* MovieFilename);
	virtual UBOOL GameThreadIsMoviePlaying(const TCHAR* MovieFilename);
	virtual FString GameThreadGetLastMovieName();
	virtual void GameThreadInitiateStartupSequence();
	virtual INT GameThreadGetCurrentFrame();
	virtual void GameThreadRequestDelayedStopMovie() { GameThreadStopMovie(TRUE); }
	virtual void GameThreadToggleInputProcessing( UBOOL bShouldMovieProcessInput );
	virtual void GameThreadSetMovieHidden( UBOOL bInHidden ) { }
	virtual void ReleaseDynamicResources();
	virtual UBOOL InputKey(FViewport* Viewport,INT ControllerId,FName Key,EInputEvent Event,FLOAT AmountDepressed,UBOOL bGamepad);
	virtual UBOOL InputTouch(FViewport* Viewport, INT ControllerId, UINT Handle, BYTE Type, FVector2D TouchLocation, DOUBLE DeviceTimestamp, UINT TouchpadIndex=0);

	// new functions
	UBOOL PlayMovie(const TCHAR* MovieFilename, INT bIsSkippable);
	UBOOL ProcessNextStartupSequence();

	//finished the movie on the mobile platform
	void CALLBACK_MovieFinished()
	{
		//make sure we didn't in fact think it was playing
		check(bIsMoviePlaying);
		bIsMoviePlaying = FALSE;
	}

	FAndroidFullScreenMovie(); 
	
	////////////////////////////////////
	/// Class: 
	///    class FStartupMovie
	///    
	/// Description: 
	///    
	///    
	////////////////////////////////////
	class FStartupMovie
	{
	public:
		FStartupMovie(const FString& InMovieName);	
		const FString MovieName;		
	};

	// current movie name
	FString					MovieName;
	// whether the game viewport is active
	UBOOL					bDisabledGameViewport;
	// are we done with startup?
	UBOOL					bProcessSequenceDone;
	// list of startup movie names. from config 
	TArray<FString>			StartupMovieNames;
	// list of movie names that stay resident. from config 
	TArray<FString>			AlwaysLoadedMovieNames;
	// list of skippable movie names. from config 
	TArray<FString>			SkippableMovieNames;
	// List of movies which are loaded and played at startup 
	TArray<FStartupMovie>	StartupMovies;	
	// Where are we in the startup sequence (-1 means normal playback) 
	INT						StartupSequenceStep;	
	// Where are we in the startup sequence (-1 means normal playback) 
	UBOOL					bIsWaitingForEndOfRequiredMovies;
	// TODO set this up to avoid calling mobile reflections
	UBOOL					bIsMoviePlaying;
	//
	DOUBLE					LastMovieStartTime;

protected:
	void SkipMovie();

	// whether the movie can process input
	UBOOL bEnableInputProcessing;
};

#endif //_FULLSCREENMOVIEFALLBACK_H_

