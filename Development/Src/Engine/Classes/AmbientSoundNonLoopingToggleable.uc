/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
 
// Version of AmbientSoundToggleable that picks a random non-looping sound to play.

class AmbientSoundNonLoopingToggleable extends AmbientSoundSimpleToggleable
	native( Sound );

defaultproperties
{
	DrawScale=1.0

	Begin Object NAME=Sprite
		Sprite=Texture2D'EditorResources.AmbientSoundIcons.S_Ambient_Sound_Non_Loop'
		Scale=0.25
	End Object

	Begin Object Name=DrawSoundRadius0
		SphereColor=(R=255,G=0,B=51)
	End Object

	Begin Object Name=AudioComponent0
		bShouldRemainActiveIfDropped=true
	End Object
	
	Begin Object Class=SoundNodeAmbientNonLoopToggle Name=SoundNodeAmbientNonLoopToggle0
	End Object
	SoundNodeInstance=SoundNodeAmbientNonLoopToggle0
}
