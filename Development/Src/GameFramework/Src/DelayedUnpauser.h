/*=============================================================================
	DelayedUnpauser.h: Util to unpause game/stop movie after specific amount of time.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __DELAYEDUNPAUSER_H__
#define __DELAYEDUNPAUSER_H__

/** Helper class that counts down when to unpause and stop movie. */
class FDelayedUnpauser : public FTickableObject
{
public:
	/** Time remaining before unpausing. */
	FLOAT UnPauseCountdown;

	/** Indicates we have unpaused the game. */
	UBOOL bHasUnPaused;

	/** Time remaining before stopping movie. */
	FLOAT StopMovieCountdown;

	/** Name of the movie that should be stopped. If TEXT("") then stops any currently playing movie */
	FString StopMovieName;

	/** Indicates we have stopped the movie. */
	UBOOL bHasStoppedMovie;


	FDelayedUnpauser( FLOAT InUnPause, FLOAT InStopMovie, const FString& InStopMovieName );
	virtual ~FDelayedUnpauser();


	/** Aborts any currently active unpauser */
	static void AbortPendingUnpauser();

	/** Returns TRUE if there is currently an pending unpauser active */
	static UBOOL HasPendingUnpauser();

	// FTickableObject interface

	void Tick(FLOAT DeltaTime);

	/** We should call Tick on this object */
	virtual UBOOL IsTickable() const;

	/** Need this to be ticked when paused (that is the point!) */
	virtual UBOOL IsTickableWhenPaused() const;

private:
	/** There can be only one. */
	static FDelayedUnpauser*	GDelayedUnpauser;
};

class FDelayedPauserAndUnpauser : public FDelayedUnpauser
{
public:
	FLOAT PauseCountdown;
	UBOOL bHasPaused;

	FDelayedPauserAndUnpauser(FLOAT InPause, FLOAT InUnPause, FLOAT InStopMovie, const FString& InStopMovieName);

	void Tick(FLOAT DeltaTime);
};

#endif
