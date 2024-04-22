/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
 
/**
 * The class works similar to AmbientSoundSpline, but it allows to bind many waves as sound sources (instead of single sound cue). Moreover for each wave a range on spline can be defined. 
 */

class AmbientSoundSimpleSpline extends AmbientSoundSpline
	AutoExpandCategories( AmbientSoundSpline )
	native( Sound );

/** Index of currently edited sound-slot */
var(AmbientSoundSpline) editoronly int EditedSlot<ToolTip=Index of currently edited slot.>;

cpptext
{
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);
}

defaultproperties
{
	Begin Object NAME=Sprite
		Sprite=Texture2D'EditorResources.AmbientSoundIcons.S_Ambient_Sound_Simple'
		Scale=0.25
	End Object

	Components.Remove( AudioComponent1 )

	Begin Object Name=DrawSoundRadius0
		SphereColor=(R=0,G=102,B=255)
	End Object

	Begin Object Class=SimpleSplineAudioComponent Name=AudioComponent2
		PreviewSoundRadius=DrawSoundRadius0
		bAutoPlay=false
		bStopWhenOwnerDestroyed=true
		bShouldRemainActiveIfDropped=true
	End Object
	AudioComponent=AudioComponent2
	Components.Add(AudioComponent2)	
}
