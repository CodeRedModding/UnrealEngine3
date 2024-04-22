/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

// An ambient sound that can be turned on or off

class AmbientSoundSimpleToggleable extends AmbientSoundSimple
	AutoExpandCategories( AmbientSoundSimpleToggleable )
	native( Sound );

/** used to update status of toggleable level placed ambient sounds on clients */
var repnotify bool bCurrentlyPlaying;

var() bool bFadeOnToggle;
var() float FadeInDuration;
var() float FadeInVolumeLevel;
var() float FadeOutDuration;
var() float FadeOutVolumeLevel;

/** Used to track whether the sound's auto-play setting should be ignored or not */
var transient bool bIgnoreAutoPlay;

struct CheckpointRecord
{
	var bool bCurrentlyPlaying;
};

replication
{
	if( Role == ROLE_Authority )
		bCurrentlyPlaying;
}

simulated event PostBeginPlay()
{
	Super.PostBeginPlay();

	bCurrentlyPlaying = AudioComponent.bAutoPlay;
}

simulated event ReplicatedEvent(name VarName)
{
	if( VarName == 'bCurrentlyPlaying' )
	{
		if( bCurrentlyPlaying )
		{
			StartPlaying();
		}
		else
		{
			StopPlaying();
		}
	}
	else
	{
		Super.ReplicatedEvent( VarName );
	}
}

simulated function StartPlaying()
{
	if( bFadeOnToggle )
	{
		AudioComponent.FadeIn( FadeInDuration, FadeInVolumeLevel );
	}
	else
	{
		AudioComponent.Play();
	}
	
	bCurrentlyPlaying = TRUE;
}

simulated function StopPlaying()
{
	if( bFadeOnToggle )
	{
		AudioComponent.FadeOut( FadeOutDuration, FadeOutVolumeLevel );
	}
	else
	{
		AudioComponent.Stop();
	}
	
	bCurrentlyPlaying = FALSE;
}

/**
 * Handling Toggle event from Kismet.
 */
simulated function OnToggle( SeqAct_Toggle Action )
{
	if( Action.InputLinks[0].bHasImpulse || ( Action.InputLinks[2].bHasImpulse && !AudioComponent.bWasPlaying ) )
	{
		StartPlaying();
	}
	else
	{
		// The sound has been intentionally toggled off, ignore autoplay from now on
		bIgnoreAutoPlay = TRUE;
		StopPlaying();
	}
	
	// we now need to replicate this Actor so clients get the updated status
	ForceNetRelevant();
}

function CreateCheckpointRecord( out CheckpointRecord Record )
{
	Record.bCurrentlyPlaying = bCurrentlyPlaying;
}

function ApplyCheckpointRecord( const out CheckpointRecord Record )
{
	bCurrentlyPlaying = Record.bCurrentlyPlaying;
	if( bCurrentlyPlaying )
	{
		StartPlaying();
	}
	else
	{
		StopPlaying();
	}
}

defaultproperties
{
	Begin Object NAME=Sprite
		Sprite=Texture2D'EditorResources.AmbientSoundIcons.S_Ambient_Sound_Toggleable'
		Scale=0.25
	End Object

	Begin Object Name=DrawSoundRadius0
		SphereColor=(R=255,G=255,B=102)
	End Object

	bAutoPlay=FALSE
	bStatic=false
	bNoDelete=true
	bIgnoreAutoPlay=FALSE

	FadeInDuration=1.f
	FadeInVolumeLevel=1.f
	FadeOutDuration=1.f
	FadeOutVolumeLevel=0.f
}
