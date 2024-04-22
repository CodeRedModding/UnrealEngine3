/**
 * GamePlayerController
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

class GamePlayerController extends PlayerController
	dependson(GamePawn)
	config(Game)
	native
	abstract;


/** If true, warn GameCrowdAgents to avoid pawn controlled by this player.  Used when player will move through crowds */
var bool bWarnCrowdMembers;
/** If TRUE, draw debug info for crowd awareness checks */
var(Debug) bool bDebugCrowdAwareness;

/** Controls how far around a player to warn agents. */
var	float	AgentAwareRadius;


/** Name of the current sound mode that is active for the audio system **/
var transient protected name CurrentSoundMode;

/** If true we are in 'warmup paused' state to allow textures to stream in, and should not allow unpausing. */
var transient bool bIsWarmupPaused;

cpptext
{
	virtual void TickSpecial( FLOAT DeltaSeconds );
}


function OnToggleMouseCursor( SeqAct_ToggleMouseCursor inAction )
{
	local GameViewportClient GVC;

	GVC = LocalPlayer(Player) != None ? LocalPlayer(Player).ViewportClient : None;
	if ( GVC != None )
	{
		GVC.SetHardwareMouseCursorVisibility(inAction.InputLinks[0].bHasImpulse);
	}
}


/**
 * @return Returns the index of this PC in the GamePlayers array.
 */
native function int GetUIPlayerIndex();

exec function CrowdDebug(bool bEnabled)
{
	local GameCrowdAgent GCA;
	local int i, AgentCount;
	local float DebugRadius;

	`log("CROWDDEBUG"@MyHUD@bEnabled);
	MyHUD.bShowOverlays = bEnabled;

	// first remove currently enabled agents, so postrendered list doesn't grow too large and kill performance
	for ( i=0; i<MyHUD.PostRenderedActors.Length; i++ )
	{
		GCA = GameCrowdAgent(MyHUD.PostRenderedActors[i]);
		if ( GCA != None )
		{
			MyHUD.RemovePostRenderedActor(GCA);
		}
	}

	if ( bEnabled )
	{
		// First count agents: don't want more than about 100 being postrendered
		DebugRadius = 2000.0;
		ForEach VisibleActors(class'GameCrowdAgent', GCA, DebugRadius, (Pawn != None) ? Pawn.Location : Location )
		{
			AgentCount++;
		}
		if( AgentCount > 100 )
		{
			// too many agents - reduce debugradius proportionally
			DebugRadius *= sqrt(100.0/float(AgentCount));
		}

		ForEach VisibleActors(class'GameCrowdAgent', GCA, DebugRadius, (Pawn != None) ? Pawn.Location : Location )
		{
			MyHUD.AddPostRenderedActor(GCA);
		}
	}
}

/**	Hook to let player know a refresh of crowd agent info is coming */
event NotifyCrowdAgentRefresh();
/** Hook to let player know crowd member is in range */
event NotifyCrowdAgentInRadius( GameCrowdAgent Agent );

/**
 *  Play forcefeedback to match the given screenshake.
 */
simulated protected function DoForceFeedbackForScreenShake( CameraShake ShakeData, float Scale )
{
	local int ShakeLevel;
	local float RotMag, LocMag, FOVMag;

	if (ShakeData != None)
	{
		// figure out if we're "big", "medium", or nothing
		RotMag = ShakeData.GetRotOscillationMagnitude() * Scale;
		if (RotMag > 40.f)
		{
			ShakeLevel = 2;
		}
		else if (RotMag > 20.f)
		{
			ShakeLevel = 1;
		}

		if (ShakeLevel < 2)
		{
			LocMag = ShakeData.GetLocOscillationMagnitude() * Scale;
			if (LocMag > 10.f)
			{
				ShakeLevel = 2;
			}
			else if (LocMag > 5.f)
			{
				ShakeLevel = 1;
			}

			FOVMag = ShakeData.FOVOscillation.Amplitude * Scale;
			if (ShakeLevel < 2)
			{
				if (FOVMag > 5.f)
				{
					ShakeLevel = 2;
				}
				else if (FOVMag > 2.f)
				{
					ShakeLevel = 1;
				}
			}
		}

		//`log("level="@ShakeLevel@"rotmag"@VSize(ShakeData.RotAmplitude)@"locmag"@VSize(ShakeData.LocAmplitude)@"fovmag"@ShakeData.FOVAmplitude);

		if (ShakeLevel == 2)
		{
			if( ShakeData.OscillationDuration <= 1 )
			{
				ClientPlayForceFeedbackWaveform(class'GameWaveForms'.default.CameraShakeBigShort);
			}
			else
			{
				ClientPlayForceFeedbackWaveform(class'GameWaveForms'.default.CameraShakeBigLong);
			}
		}
		else if (ShakeLevel == 1)
		{
			if( ShakeData.OscillationDuration <= 1 )
			{
				ClientPlayForceFeedbackWaveform(class'GameWaveForms'.default.CameraShakeMediumShort);
			}
			else
			{
				ClientPlayForceFeedbackWaveform(class'GameWaveForms'.default.CameraShakeMediumLong);
			}
		}
	}
}



/** Set the sound mode of the audio system for special EQing **/
simulated function SetSoundMode( Name InSoundModeName )
{
	local AudioDevice Audio;
	local bool bSet;

	Audio = class'Engine'.static.GetAudioDevice();
	if( Audio != None )
	{
		if( CurrentSoundMode != InSoundModeName )
		{
			bSet = Audio.SetSoundMode( InSoundModeName );
			if( bSet == TRUE )
			{
				CurrentSoundMode = InSoundModeName;
			}
		}
	}
}


/**
 * Starts/stops the loading movie
 *
 * @param bShowMovie true to show the movie, false to stop it
 * @param bPauseAfterHide (optional) If TRUE, this will pause the game/delay movie stop to let textures stream in
 * @param PauseDuration (optional) allows overriding the default pause duration specified in .ini (only used if bPauseAfterHide is true)
 * @param KeepPlayingDuration (optional) keeps playing the movie for a specified more seconds after it's supposed to stop
 * @param bOverridePreviousDelays (optional) whether to cancel previous delayed StopMovie or not (defaults to FALSE)
 */
native static final function ShowLoadingMovie(bool bShowMovie, optional bool bPauseAfterHide, optional float PauseDuration, optional float KeepPlayingDuration, optional bool bOverridePreviousDelays);

/**
 * Keep playing the loading movie if it's currently playing and abort any StopMovie calls that may be pending through the FDelayedUnpauser.
 */
native static final function KeepPlayingLoadingMovie();


/** starts playing the specified movie */
native final reliable client event ClientPlayMovie(string MovieName, int InStartOfRenderingMovieFrame, int InEndOfRenderingMovieFrame, bool bRestrictPausing , bool bPlayOnceFromStream, bool bOnlyBackButtonSkipsMovie);

/**
 * Stops the currently playing movie
 *
 * @param	DelayInSeconds			number of seconds to delay before stopping the movie.
 * @param	bAllowMovieToFinish		indicates whether the movie should be stopped immediately or wait until it's finished.
 * @param	bForceStopNonSkippable	indicates whether a movie marked as non-skippable should be stopped anyway; only relevant if the specified
 *									movie is marked non-skippable (like startup movies).
 * @param	bForceStopLoadingMovie	If false then don't stop the movie if it's the loading movie.
 */
native final reliable client event ClientStopMovie(float DelayInSeconds, bool bAllowMovieToFinish, bool bForceStopNonSkippable, bool bForceStopLoadingMovie);


/** returns the name of the currently playing movie, or an empty string if no movie is currently playing
 * @todo: add an out param for current time in playback for synchronizing clients that join in the middle
 */
native final function GetCurrentMovie(out string MovieName);


/** Delegate used to control whether we can unpause during warmup. We never allow this. */
function bool CanUnpauseWarmup()
{
	return !bIsWarmupPaused;
}


/** Function used by loading code to pause and unpause the game during texture-streaming warm-up. */
event WarmupPause(bool bDesiredPauseState)
{
	local color FadeColor;
	local PlayerController PC;
	local string MovieName;

	bIsWarmupPaused = bDesiredPauseState;

	SetPause(bDesiredPauseState, CanUnpauseWarmup);

	// When pausing, start a fade in
	// the fade won't actually go away until after the pause is turned off,
	// but starting it on pause instead of unpause prevents any delays between the loading movie vanishing and pause being disabled
	// from causing us to be viewing some random place in the world for a few frames
	if (!bDesiredPauseState)
	{
		GetCurrentMovie(MovieName);
		if (MovieName != "")
		{
			foreach LocalPlayerControllers(class'PlayerController', PC)
			{
				GamePlayerController(PC).ClientColorFade(FadeColor, 255, 0, 2.0);
			}
		}
	}
}

reliable client function ClientColorFade(Color FadeColor, byte FromAlpha, byte ToAlpha, float FadeTime);


defaultproperties
{
	AgentAwareRadius=200.0
	CheatClass=class'GameCheatManager'
}

