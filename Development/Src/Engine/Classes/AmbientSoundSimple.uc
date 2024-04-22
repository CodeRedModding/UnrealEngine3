/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
 
 // A simplified ambient sound actor for enhanced workflow
 
class AmbientSoundSimple extends AmbientSound
	hidecategories( Audio )
	AutoExpandCategories( AmbientSoundSimple )
	native( Sound );

/** Mirrored property for easier editability, set in Spawned.		*/
var()	editinline editconst	SoundNodeAmbient	AmbientProperties;
/** Dummy sound cue property to force instantiation of subobject.	*/
var		editinline export const SoundCue			SoundCueInstance;
/** Dummy sound node property to force instantiation of subobject.	*/
var		editinline export const SoundNodeAmbient	SoundNodeInstance;

defaultproperties
{
	Begin Object NAME=Sprite
		Sprite=Texture2D'EditorResources.AmbientSoundIcons.S_Ambient_Sound_Simple'
		Scale=0.25
	End Object

	Begin Object Name=DrawSoundRadius0
		SphereColor=(R=0,G=102,B=255)
	End Object

	Begin Object Class=SoundNodeAmbient Name=SoundNodeAmbient0
	End Object
	SoundNodeInstance=SoundNodeAmbient0

	Begin Object Class=SoundCue Name=SoundCue0
		SoundClass=Ambient
	End Object
	SoundCueInstance=SoundCue0
}
