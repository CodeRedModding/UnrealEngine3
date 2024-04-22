/*=============================================================================
DelayedUnpauser.h: Util to unpause game/stop movie after specific amount of time.
Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "GameFramework.h"
#include "DelayedUnpauser.h"

/** There can be only one. */
FDelayedUnpauser*	FDelayedUnpauser::GDelayedUnpauser = NULL;

FDelayedUnpauser::FDelayedUnpauser( FLOAT InUnPause, FLOAT InStopMovie, const FString& InStopMovieName )
: UnPauseCountdown(InUnPause)
, bHasUnPaused(FALSE)
, StopMovieCountdown(InStopMovie)
, StopMovieName(InStopMovieName)
, bHasStoppedMovie(FALSE)
{
	// Abort any currently active unpauser.
	AbortPendingUnpauser();

	GDelayedUnpauser = this;

	if ( appIsNearlyZero(InUnPause) )
	{
		bHasUnPaused = TRUE;
	}
}

FDelayedUnpauser::~FDelayedUnpauser()
{
	check( GDelayedUnpauser == this );
	GDelayedUnpauser = NULL;
}


/** Aborts any currently active unpauser */
void FDelayedUnpauser::AbortPendingUnpauser()
{
	if ( GDelayedUnpauser )
	{
		delete GDelayedUnpauser;
		GDelayedUnpauser = NULL;
	}
}

/** Returns TRUE if there is currently an pending unpauser active */
UBOOL FDelayedUnpauser::HasPendingUnpauser()
{
	return GDelayedUnpauser ? TRUE : FALSE;
}

/** Called each frame to decrement timers. */
void FDelayedUnpauser::Tick(FLOAT DeltaTime)
{
	UnPauseCountdown -= DeltaTime;
	StopMovieCountdown -= DeltaTime;

	//debugf(TEXT("DeltaTime:%f"), DeltaTime);
	//debugf(TEXT("UnPauseCountdown:%f"), UnPauseCountdown);


	// If we have hit zero - unpause the game (or at least try to)
	if(!bHasUnPaused && UnPauseCountdown <= 0.f)
	{
		//debugf(TEXT("UNPAUSE"));

		ULocalPlayer* LP = GEngine->GamePlayers(0);
		if(LP && LP->Actor)
		{
			AGamePlayerController* WPC = CastChecked<AGamePlayerController>(LP->Actor);
			WPC->eventWarmupPause(FALSE);
		}

		// Re-enable rumble when we unpause the game
		for(INT i=0; i<GEngine->GamePlayers.Num(); i++)
		{
			ULocalPlayer* LP = GEngine->GamePlayers(i);
			if(LP && LP->Actor && LP->Actor->ForceFeedbackManager)
			{
				LP->Actor->ForceFeedbackManager->bIsPaused = FALSE;
			}
		}

		bHasUnPaused = TRUE;				
	}

	// If this counter has hit zero - stop the movie
	if( GFullScreenMovie && 
		!bHasStoppedMovie && StopMovieCountdown <= 0.f )
	{
		//debugf(TEXT("STOP MOVIE"));

		if( GFullScreenMovie->GameThreadIsMoviePlaying(*StopMovieName) )
		{
			debugf(NAME_DevMovie, TEXT("FDelayedUnpauser stopping movie %s"),*StopMovieName);

			// tell the movie to stop after a delay so we can stream in some textures
			GFullScreenMovie->GameThreadStopMovie();
		}
		bHasStoppedMovie = TRUE;
	}

	// If we have done both tasks - kill this object
	if(bHasUnPaused && bHasStoppedMovie)
	{
		// Delete this object- destructor will remove this object from tickable object array
		delete this;
	}
}

/** We should call Tick on this object */
UBOOL FDelayedUnpauser::IsTickable() const
{
	return TRUE;
}

/** Need this to be ticked when paused (that is the point!) */
UBOOL FDelayedUnpauser::IsTickableWhenPaused() const
{
	return TRUE;
}


FDelayedPauserAndUnpauser::FDelayedPauserAndUnpauser(FLOAT InPause, FLOAT InUnPause, FLOAT InStopMovie, const FString& InStopMovieName)
:	FDelayedUnpauser(InUnPause, InStopMovie, InStopMovieName)
,	PauseCountdown(InPause)
,	bHasPaused(FALSE)
{
	if ( appIsNearlyZero(InPause) || appIsNearlyZero(InUnPause) )
	{
		bHasPaused = TRUE;
	}
}

/** Called each frame to decrement timers. */
void FDelayedPauserAndUnpauser::Tick(FLOAT DeltaTime)
{
	PauseCountdown -= DeltaTime;

	// is it time to pause yet?
	if (!bHasPaused && PauseCountdown <= 0.0f)
	{
		if(GEngine && GEngine->GamePlayers(0) && GEngine->GamePlayers(0)->Actor)
		{
			// if so pause it
			AGamePlayerController* WPC = CastChecked<AGamePlayerController>(GEngine->GamePlayers(0)->Actor);
			WPC->eventWarmupPause(TRUE);
//			check(GWorld->IsPaused());
		}

		// remember that we have paused, and we won't ever again
		bHasPaused = TRUE;
	}

	// tick the base dude
	FDelayedUnpauser::Tick(DeltaTime);
}
