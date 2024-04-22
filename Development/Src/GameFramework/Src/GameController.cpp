/**
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#include "GameFramework.h"
#include "DelayedUnpauser.h"

IMPLEMENT_CLASS(AGamePlayerController);

void AGamePlayerController::TickSpecial( FLOAT DeltaSeconds )
{
	Super::TickSpecial(DeltaSeconds);

	// tell crowd members to avoid me
	if ( bWarnCrowdMembers && Pawn )
	{
		// adjust warning location based on velocity
		FVector AwareLoc = Pawn->Location + Pawn->Velocity;

		// also increase awareness radius if moving fast
		FLOAT AwareRadius = ::Max(AgentAwareRadius, Pawn->Velocity.Size());

#if !FINAL_RELEASE
		if( bDebugCrowdAwareness )
		{
			DrawDebugBox( AwareLoc, FVector(AwareRadius), 255, 0, 0, FALSE );
		}
#endif
		eventNotifyCrowdAgentRefresh();

		FMemMark Mark(GMainThreadMemStack);
		FCheckResult* Link = GWorld->Hash->ActorOverlapCheck(GMainThreadMemStack, Pawn, AwareLoc, AwareRadius);
		for( FCheckResult* result=Link; result; result=result->GetNext())
		{
			checkSlow(result->Actor);

			// Look for nearby agents, find average velocity, and repel from them
			AGameCrowdAgent* FlockActor = Cast<AGameCrowdAgent>(result->Actor);
			if(FlockActor)
			{
				eventNotifyCrowdAgentInRadius( FlockActor );
			}
		}
		Mark.Pop();
	}
}


/**
 * Aborts any StopMovie calls that may be pending through the FDelayedUnpauser.
 */
void AGamePlayerController::KeepPlayingLoadingMovie()
{
	// Abort any currently active unpauser
	if ( FDelayedPauserAndUnpauser::HasPendingUnpauser() )
	{
		debugf(NAME_DevMovie, TEXT("Aborting the current FDelayedUnpauser"));
		FDelayedPauserAndUnpauser::AbortPendingUnpauser();
	}
}



/**
 * Starts/stops the loading movie
 *
 * @param bShowMovie true to show the movie, false to stop it
 * @param bPauseAfterHide (optional) If TRUE, this will pause the game/delay movie stop to let textures stream in
 * @param PauseDuration (optional) allows overriding the default pause duration specified in .ini (only used if bPauseAfterHide is true)
 * @param KeepPlayingDuration (optional) keeps playing the movie for a specified more seconds after it's supposed to stop
 */
void AGamePlayerController::ShowLoadingMovie(UBOOL bShowMovie, UBOOL bPauseAfterHide, FLOAT PauseDuration, FLOAT KeepPlayingDuration, UBOOL bOverridePreviousDelays)
{
	if (bShowMovie || bOverridePreviousDelays)
	{
		// Abort any currently active unpauser
		KeepPlayingLoadingMovie();
	}

	if (bShowMovie)
	{
		debugf(NAME_DevMovie, TEXT("ShowLoadingMovie(TRUE)"));
		if( GFullScreenMovie && 
			GFullScreenMovie->GameThreadIsMoviePlaying(UCONST_LOADING_MOVIE) == FALSE )
		{
//			debugf(TEXT("*****************************************************    SHOW LOADINGMOVIE (bPauseAfterHide:%s PauseDuration:%f)    *****************************************************"), bPauseAfterHide ? GTrue : GFalse, PauseDuration);
			if ( GEngine )
			{
				GEngine->PlayLoadingMovie( UCONST_LOADING_MOVIE );
			}
			else
			{
				GFullScreenMovie->GameThreadPlayMovie(MM_LoopFromMemory, UCONST_LOADING_MOVIE);
			}
		}
	}
	else
	{
		if( GFullScreenMovie && 
			GFullScreenMovie->GameThreadIsMoviePlaying(UCONST_LOADING_MOVIE) == TRUE &&
			(!bPauseAfterHide || !FDelayedPauserAndUnpauser::HasPendingUnpauser()) )
		{
//			debugf(TEXT("#####################################################    STOP LOADINGMOVIE (bPauseAfterHide:%s PauseDuration:%f)    #####################################################"), bPauseAfterHide ? GTrue : GFalse, PauseDuration);
			// if we want to pause after hiding the movie, we use the elayed pauser/unpauser so we stay unpaused
			// for a couple of frames, and then pause while streaming textures, then unpause again
			if (bPauseAfterHide)
			{
				// leave movie playing longer with game paused for textures to stream in.
				if (PauseDuration <= 0.0f)
				{
					verify(GConfig->GetFloat(TEXT("StreamByURL"), TEXT("PostLoadPause"), PauseDuration, GEngineIni));
				}

				// pause while streaming textures.
				if(GEngine && GEngine->GamePlayers(0) && GEngine->GamePlayers(0)->Actor)
				{
					GEngine->GamePlayers(0)->Actor->eventConditionalPause(TRUE);
				}

				debugf(NAME_DevMovie, TEXT("Launching a FDelayedUnpauser (PauseDuration=%.1fs KeepPlayingDuration=%.1fs)"), PauseDuration, KeepPlayingDuration);
				new FDelayedPauserAndUnpauser(0.1f, PauseDuration, PauseDuration + 0.1 + KeepPlayingDuration, FString(UCONST_LOADING_MOVIE));
			}
			// Are we supposed to keep playing the loading movie for a bit (and not pause the game)?
			else if ( appIsNearlyZero(KeepPlayingDuration) == FALSE )
			{
				debugf(NAME_DevMovie, TEXT("Launching a FDelayedUnpauser (PauseDuration=0.0s KeepPlayingDuration=%.1fs)"), KeepPlayingDuration);
				new FDelayedPauserAndUnpauser(0.0f, 0.0f, 0.1 + KeepPlayingDuration, FString(UCONST_LOADING_MOVIE));
			}
			// otherwise, just stop it now
			else
			{
				GFullScreenMovie->GameThreadStopMovie();
			}
		}
	}
}



void AGamePlayerController::ClientPlayMovie(const FString& MovieName, INT InStartOfRenderingMovieFrame, INT InEndOfRenderingMovieFrame, UBOOL bRestrictPausing, UBOOL bPlayOnceFromStream, UBOOL bOnlyBackButtonSkipsMovie )
{
	// make sure to kill loading movie first, if necessary
	ShowLoadingMovie(FALSE);

	if( GFullScreenMovie )
	{
		UINT MovieFlags = 0;

		if (bPlayOnceFromStream) 
		{
			MovieFlags |= MM_PlayOnceFromStream;
		}

		if (bOnlyBackButtonSkipsMovie) 
		{
			MovieFlags |= MF_OnlyBackButtonSkipsMovie;
		}

		// Don't allow clients to pause movies.  Only the host can do that, and only if there
		// are no remote players.
		UBOOL bAllowPausingDuringMovies = ( GWorld == NULL || GWorld->GetNetMode() != NM_Client );

		// Check player controllers to make sure they're all local.  If we have any remote players
		// then we won't allow the host to pause movies
		if( GWorld != NULL && GWorld->GetWorldInfo() != NULL )
		{
			AWorldInfo* WorldInfo = GWorld->GetWorldInfo();
			for( AController* CurController = WorldInfo->ControllerList;
				CurController != NULL;
				CurController = CurController->NextController )
			{
				// We only care about player controllers
				APlayerController* PlayerController = Cast< APlayerController >( CurController );
				if( PlayerController != NULL )
				{
					if( !PlayerController->IsLocalPlayerController() )
					{
						// Remote player, so disallow pausing
						bAllowPausingDuringMovies = FALSE;
					}
				}
			}
		}

		if( !bRestrictPausing && bAllowPausingDuringMovies )
		{
			MovieFlags |= MF_AllowUserToPause;
		}

		GFullScreenMovie->GameThreadPlayMovie( ( EMovieMode )MovieFlags, *MovieName, 0, InStartOfRenderingMovieFrame, InEndOfRenderingMovieFrame);
	}
}

/**
* Stops the currently playing movie
*
* @param	DelayInSeconds			number of seconds to delay before stopping the movie.
* @param	bAllowMovieToFinish		indicates whether the movie should be stopped immediately or wait until it's finished.
* @param	bForceStopNonSkippable	indicates whether a movie marked as non-skippable should be stopped anyway; only relevant if the specified
*									movie is marked non-skippable (like startup movies).
*/
void AGamePlayerController::ClientStopMovie( FLOAT DelayInSeconds/*=0*/, UBOOL bAllowMovieToFinish/*=TRUE*/, UBOOL bForceStopNonSkippable/*=FALSE*/, UBOOL bForceStopLoadingMovie /*= TRUE*/ )
{
	if (GFullScreenMovie != NULL)
	{
		if(!bForceStopLoadingMovie && GFullScreenMovie->GameThreadIsMoviePlaying(UCONST_LOADING_MOVIE))
		{
			return;
		}

		GFullScreenMovie->GameThreadStopMovie(DelayInSeconds, bAllowMovieToFinish, bForceStopNonSkippable);
	}
}

void AGamePlayerController::GetCurrentMovie(FString& MovieName)
{
	//@todo: need to get current time in movie, too
	if( GFullScreenMovie && 
		GFullScreenMovie->GameThreadIsMoviePlaying(TEXT("")) )
	{
		MovieName = GFullScreenMovie->GameThreadGetLastMovieName();
	}
	else
	{
		MovieName = TEXT("");
	}
}

/** @return The index of the PC in the game player's array. */
INT AGamePlayerController::GetUIPlayerIndex()
{
	ULocalPlayer* LP = NULL;
	INT Result = INDEX_NONE;

	LP = Cast<ULocalPlayer>(Player);

	if(LP)
	{	
		Result = UUIInteraction::GetPlayerIndex(LP);
	}

	return Result;
}

