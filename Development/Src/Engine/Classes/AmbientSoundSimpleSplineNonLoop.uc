/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

class AmbientSoundSimpleSplineNonLoop extends AmbientSoundSimpleSpline;

defaultproperties
{
	Begin Object Name=DrawSoundRadius0
		SphereColor=(R=255,G=0,B=51)
	End Object

	Begin Object NAME=Sprite
		Sprite=Texture2D'EditorResources.AmbientSoundIcons.S_Ambient_Sound_Non_Loop'
		Scale=0.25
	End Object

	Components.Remove( AudioComponent2 )

	Begin Object Class=SimpleSplineNonLoopAudioComponent Name=AudioComponent3
		PreviewSoundRadius=DrawSoundRadius0
		bAutoPlay=false
		bStopWhenOwnerDestroyed=true
		bShouldRemainActiveIfDropped=true
	End Object

	AudioComponent=AudioComponent3
	Components.Add(AudioComponent3)	
}
