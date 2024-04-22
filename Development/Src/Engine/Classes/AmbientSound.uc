/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

// Base ambient sound actor

class AmbientSound extends Keypoint
	AutoExpandCategories( Audio )
	ClassGroup(Sounds)
	native( Sound );

/** Should the audio component automatically play on load? */
var() bool bAutoPlay;

/** Audio component to play */
var( Audio ) editconst const AudioComponent AudioComponent;

/** Is the audio component currently playing? */
var private bool bIsPlaying;

defaultproperties
{
	Begin Object NAME=Sprite
		Sprite=Texture2D'EditorResources.AmbientSoundIcons.S_Ambient_Sound'
		Scale=0.25
		SpriteCategoryName="Sounds"
	End Object

	Begin Object Class=DrawSoundRadiusComponent Name=DrawSoundRadius0
		SphereColor=(R=255,G=153,B=0)
	End Object
	Components.Add(DrawSoundRadius0)
	
	Begin Object Class=AudioComponent Name=AudioComponent0
		PreviewSoundRadius=DrawSoundRadius0
		bAutoPlay=false
		bStopWhenOwnerDestroyed=true
		bShouldRemainActiveIfDropped=true
	End Object
	AudioComponent=AudioComponent0
	Components.Add(AudioComponent0)

	bAutoPlay=TRUE
	
	RemoteRole=ROLE_None
}
