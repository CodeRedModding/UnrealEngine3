/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
 
// Version of AmbientSoundSimple that picks a random non-looping sound to play.

class AmbientSoundNonLoop extends AmbientSoundSimple
	native( Sound );

defaultproperties
{
	DrawScale=2.0

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

	Begin Object Class=SoundNodeAmbientNonLoop Name=SoundNodeAmbientNonLoop0
	End Object
	SoundNodeInstance=SoundNodeAmbientNonLoop0
}
